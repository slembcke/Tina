/*
	Copyright (c) 2021 Scott Lembcke

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

// NOTE: This is an interative Mandelbrot fractal explorer built on top of Sokol.

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <tgmath.h>

#include "common/common.h"

#define SOKOL_IMPL
#define SOKOL_GL_IMPL
#define SOKOL_GLCORE33
#include "common/libs/sokol_app.h"
#include "common/libs/sokol_gfx.h"
#include "common/libs/sokol_gl.h"

#include "tina_jobs.h"

enum {
	// Used as concurrent priority queues for rendering tiles.
	QUEUE_WORK,
	// A serial queue used to upload new textures.
	QUEUE_GFX,
	QUEUE_GFX_WAIT,
	_QUEUE_COUNT,
};

tina_scheduler* SCHED;

// A bunch of random 2D affine matrix code I pulled from another project.

typedef struct {double x, y;} Vec2;
typedef struct {double a, b, c, d, x, y;} Transform;

static const Transform TRANSFORM_IDENTITY = {1, 0, 0, 1, 0, 0};

static inline Transform TransformMakeTranspose(double a, double c, double x, double b, double d, double y){
	return (Transform){a, b, c, d, x, y};
}

static inline Transform TransformMult(Transform m1, Transform m2){
  return TransformMakeTranspose(
    m1.a*m2.a + m1.c*m2.b, m1.a*m2.c + m1.c*m2.d, m1.a*m2.x + m1.c*m2.y + m1.x,
    m1.b*m2.a + m1.d*m2.b, m1.b*m2.c + m1.d*m2.d, m1.b*m2.x + m1.d*m2.y + m1.y
  );
}

static inline Transform TransformInverse(Transform m){
  double inv_det = 1/(m.a*m.d - m.c*m.b);
  return TransformMakeTranspose(
     m.d*inv_det, -m.c*inv_det, (m.c*m.y - m.d*m.x)*inv_det,
    -m.b*inv_det,  m.a*inv_det, (m.b*m.x - m.a*m.y)*inv_det
  );
}

static inline Transform TransformOrtho(const double l, const double r, const double b, const double t){
	double sx = 2/(r - l);
	double sy = 2/(t - b);
	double tx = -(r + l)/(r - l);
	double ty = -(t + b)/(t - b);
	return TransformMakeTranspose(
		sx,  0, tx,
		 0, sy, ty
	);
}

static inline Vec2 TransformPoint(Transform t, Vec2 p){
	return (Vec2){t.a*p.x + t.c*p.y + t.x, t.b*p.x + t.d*p.y + t.y};
}

static inline Vec2 TransformVec(Transform t, Vec2 p){
	return (Vec2){t.a*p.x + t.c*p.y, t.b*p.x + t.d*p.y};
}

typedef struct {float m[16];} GLMatrix;

static inline GLMatrix TransformToGL(Transform m){
	return (GLMatrix){.m = {m.a, m.b, 0, 0, m.c, m.d, 0, 0, 0, 0, 1, 0, m.x, m.y, 0, 1}};
}

// Current frame timestamp used to kill off tile requests that go offscreen before they finish.
unsigned TIMESTAMP;

typedef struct {
	int64_t x, y, z;
} tile_coord;

static inline double coord_to_scale(tile_coord c){
	return exp2(4 - c.z);
}

static inline Transform coord_to_matrix(tile_coord c){
	double s = coord_to_scale(c);
	return (Transform){s, 0, 0, s, s*c.x, s*c.y};
}

// Image tiles are arranged into a quadtree.
typedef struct tile_node tile_node;
struct tile_node {
	tile_coord coord;
	// Last frame this tile was visible.
	uint64_t timestamp;
	// Has the tile already been requested.
	bool requested;
	bool complete;
	// Texture for this tile.
	sg_image texture;
	tile_node* children;
};

// Root node of the quadtree.
static tile_node TREE_ROOT;

// Texture and cache variables.

#define TEXTURE_SIZE 256
#define TEXTURE_CACHE_SIZE 1024

// Implements a simple FIFO texture cache.

// Current cache index to replace next. (If it's not currently visible)
size_t TEXTURE_CURSOR;
// Node that owns a cache index.
tile_node* TEXTURE_NODE[TEXTURE_CACHE_SIZE];
sg_image TEXTURE_CACHE[TEXTURE_CACHE_SIZE];
unsigned TEXTURE_TIMESTAMP[TEXTURE_CACHE_SIZE];

// Context for the task to render sample batches.
typedef struct {
	tile_node* node;
	uint8_t* pixels;
} render_scanline_ctx;

// How many samples to run in a batch.
#define SAMPLE_BATCH_COUNT 256

static uint8_t decode_zcurve(uint8_t n){
	// Copy n into each byte of splat.
	uint64_t splat = n*0x0101010101010101;
	// Mask and select bits into the upper byte.
	uint64_t x = (splat & 0x8040201008040201)*0x1000080004000200;
	uint64_t y = (splat & 0x8040201008040201)*0x0008000400020001;
	// Combine
	return (x >> 60) | (y >> 56);
}

// Task function that renders mandelbrot samples.
static void render_samples_job(tina_job* job){
	const render_scanline_ctx* const ctx = tina_job_get_description(job)->user_data;
	
	// Check if the request is valid since waiting in the queue.
	if(!ctx->node->requested) return;

	tile_coord c = ctx->node->coord;
	double scale = coord_to_scale(c)/TEXTURE_SIZE;
	c.x *= TEXTURE_SIZE;
	c.y *= TEXTURE_SIZE;
	
	unsigned batch_idx = tina_job_get_description(job)->user_idx;
	// This is completely unnecessary, but looks mildly neat.
	batch_idx = decode_zcurve(batch_idx);
	int x0 = 16*(batch_idx & 0x0F), y0 = 16*(batch_idx / 0x10);
	
	double complex coords[SAMPLE_BATCH_COUNT];
	for(int i = 0; i < TEXTURE_SIZE; i++){
		int x = x0 + (i & 0x0F), y = y0 + (i / 0x10);
		coords[i] = 
			(c.x + 2*x - TEXTURE_SIZE + 1)*scale +
			(c.y + 2*y - TEXTURE_SIZE + 1)*scale*I;
	}
	
	const unsigned maxi = 32*1024;
	const double bailout = 256;
	
	float r_samples[SAMPLE_BATCH_COUNT];
	float g_samples[SAMPLE_BATCH_COUNT];
	float b_samples[SAMPLE_BATCH_COUNT];
	
	for(unsigned idx = 0; idx < SAMPLE_BATCH_COUNT; idx++){
		double complex c = coords[idx];
		double complex z = c;
		double complex dz = 1;
		
		// Iterate until the fractal function diverges.
		unsigned i = 0;
		while(fabs(z) <= bailout && i < maxi){
			dz *= 2*z;
			if(fabs(dz) < 0x1p-16){
				i = maxi;
				break;
			}
			
			z = z*z + c;
			i++;
		}
		
		// Color the pixel based on if it diverged and how.
		if(i == maxi){
			r_samples[idx] = 0;
			g_samples[idx] = 0;
			b_samples[idx] = 0;
		} else {
			double rem = 1 + log2(log2(bailout)) - log2(log2(fabs(z)));
			double n = (i - 1) + rem;
			
			double phase = 5*log2(n);
			r_samples[idx] = 0.5 + 0.5*cos(phase + 0*M_PI/3);
			g_samples[idx] = 0.5 + 0.5*cos(phase + 2*M_PI/3);
			b_samples[idx] = 0.5 + 0.5*cos(phase + 4*M_PI/3);
		}
	}
	
	// Resolve. (TODO need to move this to the GFX queue to get rid of flickering?)
	uint8_t* pixels = ctx->pixels + 4*16*((batch_idx & 0x0F) + TEXTURE_SIZE*(batch_idx / 0x10));
	for(unsigned src_idx = 0; src_idx < SAMPLE_BATCH_COUNT; src_idx++){
		float dither = 0;
		
		unsigned dst_idx = 4*((src_idx & 0xF) + TEXTURE_SIZE*(src_idx / 16));
		pixels[dst_idx + 0] = 255*fmax(0, fmin(r_samples[src_idx] + dither, 1));
		pixels[dst_idx + 1] = 255*fmax(0, fmin(g_samples[src_idx] + dither, 1));
		pixels[dst_idx + 2] = 255*fmax(0, fmin(b_samples[src_idx] + dither, 1));
		pixels[dst_idx + 3] = 255;
	}
}

static void update_tile_texture(tina_job* job, unsigned tex_id, void* pixels){
	// TODO this is kinda dumb... but whatever.
	if(TEXTURE_TIMESTAMP[tex_id] == TIMESTAMP){
		tina_job_switch_queue(job, QUEUE_GFX_WAIT);
		tina_job_switch_queue(job, QUEUE_GFX);
	}
	
	// Upload and set up the new texture.
	TEXTURE_TIMESTAMP[tex_id] = TIMESTAMP;
	sg_update_image(TEXTURE_CACHE[tex_id], &(sg_image_content){.subimage[0][0] = {.ptr = pixels}});
}

// Task function that renders mandelbrot image tiles.
static void generate_tile_job(tina_job* job){
	tile_node *node = tina_job_get_description(job)->user_data;
	unsigned queue = tina_job_get_description(job)->queue_idx;
	
	const unsigned sample_count = TEXTURE_SIZE*TEXTURE_SIZE;

	// Allocate memory for the sample coords and output values.
	// You'd probably want to batch allocate these if this was a real thing.
	uint8_t* pixels = calloc(4, TEXTURE_SIZE*TEXTURE_SIZE);
	render_scanline_ctx render_context = {.node = node, .pixels = pixels};
	
	// Create a group to act as a throttle for how many in flight subtasks are created. 
	tina_group group = {};
	
	// OK! Now for the exciting part!
	// Loop through all the pixels and subsamples (for anti-aliasing).
	// Break them into batches of SAMPLE_BATCH_COUNT size, and create tasks to render them.
	unsigned batch_cursor = 0;
	while(batch_cursor < 256){
		tina_scheduler_enqueue(SCHED, "RenderSamples", render_samples_job, &render_context, batch_cursor, queue, &group);
		batch_cursor++;
	}
	
	tina_job_switch_queue(job, QUEUE_GFX);
	
	// Linear search for the oldest texture in the cache that is no longer used.
	unsigned tex_id = TEXTURE_CURSOR;
	while(TEXTURE_NODE[tex_id] && TEXTURE_NODE[tex_id]->timestamp == TIMESTAMP){
		tex_id = (tex_id + 1) & (TEXTURE_CACHE_SIZE - 1);
	}
	TEXTURE_CURSOR = (tex_id + 1) & (TEXTURE_CACHE_SIZE - 1);
	
	// Unlink it from the previous node.
	if(TEXTURE_NODE[tex_id]){
		TEXTURE_NODE[tex_id]->texture.id = 0;
		TEXTURE_NODE[tex_id]->complete = false;
	}
	
	// Clear the texture.
	static uint8_t CLEAR[4*TEXTURE_SIZE*TEXTURE_SIZE];
	update_tile_texture(job, tex_id, CLEAR);
	
	// Link it to our node.
	TEXTURE_NODE[tex_id] = node;
	node->texture = TEXTURE_CACHE[tex_id];
	
	while(batch_cursor){
		// Check if this tile is already stale and bailout.
		if(node->timestamp + 16 < TIMESTAMP){
			node->requested = false;
			goto cleanup;
		}
		
		batch_cursor = tina_job_wait(job, &group, batch_cursor - 1);
		update_tile_texture(job, tex_id, pixels);
	}
	// tina_job_wait(job, &group, 0);
	
	update_tile_texture(job, tex_id, pixels);
	node->requested = false;
	node->complete = true;
	
	cleanup:
	// If we aborted because this was a stale tile, wait for all batches to finish before freeing their memory!
	tina_job_wait(job, &group, 0);
	free(pixels);
}

#define VIEW_RESET (Transform){0.75, 0, 0, 0.75, 0.5, 0}
static Transform proj_matrix = {1, 0, 0, 1, 0, 0};
static Transform view_matrix = VIEW_RESET;
double zoom = 0;

static Transform pixel_to_world_matrix(void){
	Transform pixel_to_clip = TransformOrtho(0, sapp_width(), sapp_height(), 0);
	Transform vp_inv_matrix = TransformInverse(TransformMult(proj_matrix, view_matrix));
	return TransformMult(vp_inv_matrix, pixel_to_clip);
}

static Vec2 mouse_pos;

static bool frustum_cull(const Transform mvp){
	// Clip space center and extents.
	Vec2 c = {mvp.x, mvp.y};
	double ex = fabs(mvp.a) + fabs(mvp.c);
	double ey = fabs(mvp.b) + fabs(mvp.d);
	
	return ((fabs(c.x) - ex < 1) && (fabs(c.y) - ey < 1));
}

static void draw_tile(tile_node* node){
	tile_coord c = node->coord;
	double s = coord_to_scale(c);
	Transform m = TransformMult(view_matrix, (Transform){s, 0, 0, s, 0, 0});
	Vec2 v00 = TransformPoint(m, (Vec2){c.x - 1, c.y - 1});
	Vec2 v10 = TransformPoint(m, (Vec2){c.x + 1, c.y - 1});
	Vec2 v01 = TransformPoint(m, (Vec2){c.x - 1, c.y + 1});
	Vec2 v11 = TransformPoint(m, (Vec2){c.x + 1, c.y + 1});
	
	sgl_texture(node->texture);
	sgl_begin_triangle_strip();
		sgl_v2f_t2f(v00.x, v00.y, 0, 0);
		sgl_v2f_t2f(v10.x, v10.y, 1, 0);
		sgl_v2f_t2f(v01.x, v01.y, 0, 1);
		sgl_v2f_t2f(v11.x, v11.y, 1, 1);
	sgl_end();
}

#define REQUEST_QUEUE_LENGTH 8

static void request_insert(tile_node** request_queue, tile_node* node){
	for(int i = 0; i < REQUEST_QUEUE_LENGTH; i++){
		tile_node* requested = request_queue[i];
		if(!requested || node->coord.z < requested->coord.z){
			request_queue[i] = node;
			node = requested;
			if(!node) break;
		}
	}
}

// Visit the image tile quadtree, recursively rendering and loading new nodes.
static bool visit_tile(tile_node* node, tile_node** request_queue){
	Transform matrix = coord_to_matrix(node->coord);
	// Check if this tile is visible on the screen before continuing.
	Transform mv_matrix = TransformMult(view_matrix, matrix);
	if(!frustum_cull(TransformMult(proj_matrix, mv_matrix))) return true;
	
	node->timestamp = TIMESTAMP;
	Transform ddm = TransformMult(TransformInverse(pixel_to_world_matrix()), matrix);
	float scale = sqrt(ddm.a*ddm.a + ddm.b*ddm.b) + sqrt(ddm.c*ddm.c + ddm.d*ddm.d);
	
	if(node->texture.id){
		unsigned child_coverage = 0;
		
		if(node->complete && scale > TEXTURE_SIZE){
			// Allocate the children if they haven't been visited yet.
			if(!node->children){
				node->children = calloc(4, sizeof(*node->children));
				
				tile_coord c = node->coord;
				node->children[0].coord = (tile_coord){2*c.x - 1, 2*c.y - 1, c.z + 1};
				node->children[1].coord = (tile_coord){2*c.x + 1, 2*c.y - 1, c.z + 1};
				node->children[2].coord = (tile_coord){2*c.x - 1, 2*c.y + 1, c.z + 1};
				node->children[3].coord = (tile_coord){2*c.x + 1, 2*c.y + 1, c.z + 1};
			}
			
			// Visit all of the children.
			child_coverage += visit_tile(node->children + 0, request_queue);
			child_coverage += visit_tile(node->children + 1, request_queue);
			child_coverage += visit_tile(node->children + 2, request_queue);
			child_coverage += visit_tile(node->children + 3, request_queue);
		}
		
		if(child_coverage < 4) draw_tile(node);
		
		return node->complete;
	} else if(!node->requested){
		request_insert(request_queue, node);
	}
	
	return false;
}

static void app_display(void){
	// Run jobs to load textures.
	tina_scheduler_run(SCHED, QUEUE_GFX_WAIT, true);
	tina_scheduler_run(SCHED, QUEUE_GFX, true);
	TIMESTAMP++;
	
	int w = sapp_width(), h = sapp_height();
	sg_pass_action action = {.colors[0] = {.action = SG_ACTION_CLEAR, .val = {0, 0, 0, 0}}};
	sg_begin_default_pass(&action, w, h);
	
	float pw = fmax(1, (float)w/(float)h);
	float ph = fmax(1, (float)h/(float)w);
	proj_matrix = TransformOrtho(-pw, pw, -ph, ph);
	
	sgl_matrix_mode_projection();
	sgl_load_matrix(TransformToGL(proj_matrix).m);
	
	float scale = exp2(-zoom/16);
	zoom *= 1 - exp2(-3);
	Vec2 mpos = TransformPoint(pixel_to_world_matrix(), mouse_pos);
	Transform t = {scale, 0, 0, scale, mpos.x*(1 - scale), mpos.y*(1 - scale)};
	view_matrix = TransformMult(view_matrix, t);
	
	tile_node* request_queue[REQUEST_QUEUE_LENGTH] = {};
	visit_tile(&TREE_ROOT, request_queue);
	
	static tina_group tile_throttle_group = {.max_count = 8};
	for(int i = 0; i < REQUEST_QUEUE_LENGTH; i++){
		if(request_queue[i] && tina_scheduler_enqueue(SCHED, "GenTiles", generate_tile_job, request_queue[i], 0, QUEUE_WORK, &tile_throttle_group)){
			// The node was successfully queued, so mark it as requested.
			request_queue[i]->requested = true;
		}
	}
	
	sgl_draw();
	sg_end_pass();
	sg_commit();
}

static bool mouse_drag;

static void app_event(const sapp_event *event){
	switch(event->type){
		case SAPP_EVENTTYPE_KEY_UP: {
			if(event->key_code == SAPP_KEYCODE_ESCAPE) sapp_request_quit();
			if(event->key_code == SAPP_KEYCODE_SPACE) view_matrix = VIEW_RESET;
		} break;
		
		case SAPP_EVENTTYPE_MOUSE_MOVE: {
			Vec2 new_pos = (Vec2){event->mouse_x, event->mouse_y};
			if(mouse_drag){
				Vec2 mouse_delta = {new_pos.x - mouse_pos.x, new_pos.y - mouse_pos.y};
				Vec2 delta = TransformVec(pixel_to_world_matrix(), mouse_delta);
				Transform t = {1, 0, 0, 1, delta.x, delta.y};
				view_matrix = TransformMult(view_matrix, t);
			}
			mouse_pos = new_pos;
		}; break;
		
		case SAPP_EVENTTYPE_MOUSE_DOWN: {
			if(event->mouse_button == SAPP_MOUSEBUTTON_LEFT) mouse_drag = true;
		} break;
		case SAPP_EVENTTYPE_MOUSE_UP: {
			if(event->mouse_button == SAPP_MOUSEBUTTON_LEFT) mouse_drag = false;
		} break;
		
		case SAPP_EVENTTYPE_MOUSE_SCROLL: {
			zoom += event->scroll_y;
		} break;
		
		default: break;
	}
}

static void app_init(void){
	puts("Sokol-App init.");
	
	puts("Creating SCHED.");
	SCHED = tina_scheduler_new(16*1024, _QUEUE_COUNT, 128, 64*1024);
	
	common_start_worker_threads(0, SCHED, QUEUE_WORK);
	
	puts("Init Sokol-GFX.");
	sg_desc gfx_desc = {.image_pool_size = TEXTURE_CACHE_SIZE + 1};
	sg_setup(&gfx_desc);
	assert(sg_isvalid());
	
	for(unsigned i = 0; i < 1024; i++){
		TEXTURE_CACHE[i] = sg_make_image(&(sg_image_desc){
			.width = TEXTURE_SIZE, .height = TEXTURE_SIZE,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.min_filter = SG_FILTER_LINEAR,
			.mag_filter = SG_FILTER_LINEAR,
			.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
			.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
			.usage = SG_USAGE_DYNAMIC,
		});
	}
	
	puts("Init Sokol-GL.");
	sgl_desc_t gl_desc = {};
	sgl_setup(&gl_desc);
	
	sgl_defaults();
	sgl_enable_texture();
	
	sgl_load_pipeline(sgl_make_pipeline(&(sg_pipeline_desc){
		.blend.enabled = true,
		.blend.src_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
		.blend.dst_factor_rgb = SG_BLENDFACTOR_DST_ALPHA,
		.blend.src_factor_alpha = SG_BLENDFACTOR_ONE,
		.blend.dst_factor_alpha = SG_BLENDFACTOR_ONE,
		.blend.color_write_mask = SG_COLORMASK_RGBA,
	}));
	
	puts("Init complete.");
}

static void app_cleanup(void){
	puts("Sokol-App cleanup.");
	// tina_job_wait_sleep(SCHED, &group, 0);
	
	puts("WORKERS shutdown.");
	TIMESTAMP += 1000;
	tina_scheduler_pause(SCHED);
	common_destroy_worker_threads();
	
	puts ("Destroing SCHED");
	tina_scheduler_destroy(SCHED);
	free(SCHED);
	
	puts("Sokol-GFX shutdown.");
	sgl_shutdown();
	sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
	return (sapp_desc){
		.init_cb = app_init,
		.frame_cb = app_display,
		.event_cb = app_event,
		.cleanup_cb = app_cleanup,
		.width = 1024,
		.height = 1024,
		.window_title = "Mandelbrot",
	};
}

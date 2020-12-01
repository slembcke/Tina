# Print out a backtrace for a suspended Tina coroutine.
define tina_backtrace_amd64
	set $_tina_sp = (void**)$arg0->_sp
	# Tina saves the base pointer 5 from the top of the stack.
	set $_tina_sp = (void**)$_tina_sp[5]
	
	# From here:
	# _tina_sp[0] is the saved base pointer
	# _tina_sp[1] is the return address
	# _tina_sp + 8 is the frame pointer
	# Loop until there is no more return addresses.
	while $_tina_sp[1]
		set $_tina_frame = $_tina_sp + 8
		frame view $_tina_frame $_tina_sp[1]
		set $_tina_sp = (void**)$_tina_sp[0]
	end
	
	# Go back to (probably?) the frame you were at.
	select-frame 0
end

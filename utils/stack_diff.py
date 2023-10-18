import lldb

def stack_diff(debugger, command, result, internal_dict):
    print("Running stack_diff...")  # Debugging output

    # Get the current thread
    thread = debugger.GetSelectedTarget().GetProcess().GetSelectedThread()

    # Get the current frame and its index
    current_frame = thread.GetSelectedFrame()
    current_frame_idx = current_frame.GetFrameID()

    # Get the parent frame using the index
    parent_frame = thread.GetFrameAtIndex(current_frame_idx + 1)

    if parent_frame is None or not parent_frame.IsValid():
        print("No parent frame available.")
        return

    # Get SP for current and parent frames
    current_sp = current_frame.GetSP()
    parent_sp = parent_frame.GetSP()

    # Calculate and print the difference
    diff = parent_sp - current_sp
    print(f"Stack Pointer Difference: {diff} bytes")

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f stack_diff.stack_diff stack_diff')
    print("stack_diff command has been loaded.")  # Debugging output

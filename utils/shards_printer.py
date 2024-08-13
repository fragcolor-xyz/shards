import lldb
import logging

# Set up logging
logging.basicConfig(level=logging.DEBUG, filename='lldb_plugin.log', filemode='w')
logger = logging.getLogger(__name__)

def seq_summary(valobj, internal_dict):
    try:
        logger.debug(f"Starting seq_summary for {valobj.GetName()}")

        elements = valobj.GetChildMemberWithName('elements')
        if not elements.IsValid():
            logger.error("'elements' member not found")
            return "Error: 'elements' member not found"

        elements_address = elements.GetValueAsUnsigned()
        length = valobj.GetChildMemberWithName('len').GetValueAsUnsigned()
        capacity = valobj.GetChildMemberWithName('cap').GetValueAsUnsigned()

        logger.debug(f"elements_address: 0x{elements_address:x}, length: {length}, capacity: {capacity}")

        element_type = elements.GetType().GetPointeeType()
        element_size = element_type.GetByteSize()

        logger.debug(f"element_type: {element_type}, element_size: {element_size}")

        elements_str = []
        if length > 0:
            for i in range(length):
                element_address = elements_address + i * element_size

                logger.debug(f"Attempting to print element at address: 0x{element_address:x}")

                # Use LLDB's built-in 'p' command
                res = lldb.SBCommandReturnObject()
                interpreter = lldb.debugger.GetCommandInterpreter()
                command = f'p (SHExposedTypeInfo*)0x{element_address:x}'
                interpreter.HandleCommand(command, res)

                if res.Succeeded():
                    output = res.GetOutput().strip()
                    elements_str.append(f"Element {i}: {output}")
                else:
                    error = res.GetError()
                    logger.error(f"Command failed with error: {error}")
                    elements_str.append(f"Element {i}: Error - {error}")

        elements_summary = '\n'.join(elements_str)
        result = f'Custom Summary: elements = 0x{elements_address:x}, len = {length}, cap = {capacity}, values =\n{elements_summary}'
        logger.debug(f"Final result: {result}")
        return result

    except Exception as e:
        logger.exception(f"Error in seq_summary: {str(e)}")
        return f"Error in seq_summary: {str(e)}"


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand(
        'type summary add -F shards_printer.seq_summary SHSeq')
    debugger.HandleCommand(
        'type summary add -F shards_printer.seq_summary SHExposedTypesInfo')
    debugger.HandleCommand(
        'type summary add -F shards_printer.seq_summary SHTypesInfo')
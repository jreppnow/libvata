import cli_operation_representation
import cli_options_enums

ENCODING_PARAM = '-r'
OPTION_PARAM = '-o'
DIR_PARAM = 'dir='
ORDER_PARAM = 'order='

def serializeEncoding(encoding):
    return [ENCODING_PARAM, cli_options_enums.EncodingToString[encoding]]

def serializeOperation(operation):
    return [cli_options_enums.InstructionsToStringCommand[operation]]

def serializeOperands(operands):
    return operands

def serializeSimulation(enc, options):
    if enc == cli_options_enums.EncodingsEnum.EXPL_FA:
        return DIR_PARAM + cli_options_enums.DirectionsEnumFAToString[options.getDirection()]
    else:
        return DIR_PARAM + cli_options_enums.DirectionsEnumToString[options.getDirection()]

def serializeReduction(enc, options):
    return DIR_PARAM + cli_options_enums.DirectionsEnumToString[options.getDirection()]

def serializeEquiv(enc, options):
    return ORDER_PARAM + cli_options_enums.OrderEnumToString[options.getOrder()]

def serializeIncl(enc, options):
    return DIR_PARAM + cli_options_enums.OrderEnumToString[options.getOrder()]

def serializeOptions(command, options):
    res = []

    if command == cli_options_enums.InstructionsEnum.EQUIV:
        res = serializeEquiv(options)
    elif command == cli_options_enums.InstructionsEnum.SIM:
        res = serializeSimulation(command.getEncoding(), options)
    elif command == cli_options_enums.InstructionsEnum.RED:
        res = serializeReduction(options)
    elif command == cli_options_enums.InstructionsEnum.INCL:
        res = serializeIncl(options)

    return [OPTION_PARAM, res]

def serializeCommand(command):
    res = serializeEncoding(command.getEncoding())
    res += serializeOperation(command.getOperation())
    if operation.getOptions() is not None:
        res += [] serializeOptions(operation.getOptions())
    res += serializeOperands(command.getOperands())
    
    return res

def serializeCommandWithOption(operation, option, lhs, rhs):
    return serializeOperation(operation) + serializeOptions(option) + serializeOperands(lhs, rhs)

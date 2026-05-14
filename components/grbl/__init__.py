from logging import config

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PORT, SCHEDULER_DONT_RUN

from .const import *

DEPENDENCIES = ['uart']
AUTO_LOAD = ['binary_sensor', 'async_tcp']

ns = cg.esphome_ns.namespace('grbl')
uart_ns = cg.esphome_ns.namespace("uart")

Grbl = ns.class_('Grbl', cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Grbl),
    cv.Optional(CONF_PORT, default=23): cv.port,
    cv.Optional(CONF_ALLOW_COMMANDS_WHEN_CONNECTED, default=False): cv.boolean,
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_allow_commands_when_connected(config[CONF_ALLOW_COMMANDS_WHEN_CONNECTED]))

    yield uart.register_uart_device(var, config)


## Tools


def grbl_param(value):
    if isinstance(value, int):
        return value

    if isinstance(value, str) and value.startswith("$"):
        try:
            return int(value[1:])
        except ValueError:
            pass

    raise cv.Invalid("GRBL parameter must be an integer or '$<number>'")


def float_or_auto(value):
    if isinstance(value, str) and value.lower() == CONF_AUTO:
        return -1.
    return cv.float_(value)


class ValidateAxisDependency:
    def __init__(self, *param_keys):
        self.param_keys = param_keys

    def __call__(self, config):
        param = config.get(CONF_ENTITY_PARAM)
        has_axis = CONF_AXIS in config

        if param in self.param_keys:
            if not has_axis:
                raise cv.Invalid(f"axis is required when param is '{param}'")
        else:
            if has_axis:
                raise cv.Invalid(f"axis not allowed when param is '{param}'")

        return config


GrblCoords = ns.enum('Coords')

GRBL_COORDS = {
    'G92': GrblCoords.COORDS_G92,
    'G54': GrblCoords.COORDS_G54,
    'G55': GrblCoords.COORDS_G55,
    'G56': GrblCoords.COORDS_G56,
    'G57': GrblCoords.COORDS_G57,
    'G58': GrblCoords.COORDS_G58,
    'G59': GrblCoords.COORDS_G59,
}


## Actions

GRBL_SIMPLE_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
})


def register_grbl_simple_action(action_name, class_name):
    cls = ns.class_(
        class_name,
        automation.Action,
        cg.Parented.template(Grbl),
    )

    @automation.register_action(action_name, cls, GRBL_SIMPLE_ACTION_SCHEMA, synchronous=True)
    async def action_to_code(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg)
        await cg.register_parented(var, config[CONF_GRBL_ID])
        return var

    return cls


# grbl.cancel_jog, grbl.reset, grbl.release_state, grbl.run_homing_cycle, and grbl.update_status actions

GrblCancelJogAction = register_grbl_simple_action(ACTION_CANCEL_JOG, "GrblCancelJogAction")
GrblSendResetAction = register_grbl_simple_action(ACTION_RESET, "GrblSendResetAction")
GrblReleaseStateAction = register_grbl_simple_action(ACTION_RELEASE_STATE, "GrblReleaseStateAction")
GrblRunHomingCycleAction = register_grbl_simple_action(ACTION_RUN_HOMING_CYCLE, "GrblRunHomingCycleAction")
GrblUpdateStatusAction = register_grbl_simple_action(ACTION_UPDATE_STATUS, "GrblUpdateStatusAction")

# grbl.send_command action

GrblSendCommandAction = ns.class_("GrblSendCommandAction", automation.Action, cg.Parented.template(Grbl))

GRBL_SEND_COMMAND_SCHEMA = cv.maybe_simple_value(
    cv.Schema({
        cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    }),
    key=CONF_COMMAND,
)


@automation.register_action(ACTION_SEND_COMMAND, GrblSendCommandAction, GRBL_SEND_COMMAND_SCHEMA, synchronous=True)
async def grbl_send_command_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_GRBL_ID])
    command = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(command))
    return var


# grbl.jog action

GrblJogAction = ns.class_("GrblJogAction", automation.Action, cg.Parented.template(Grbl))

GRBL_JOG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Optional(CONF_DX, default=0.0): cv.templatable(cv.float_),
    cv.Optional(CONF_DY, default=0.0): cv.templatable(cv.float_),
    cv.Optional(CONF_DZ, default=0.0): cv.templatable(cv.float_),
    cv.Optional(CONF_SPEED, default=1000): cv.templatable(cv.int_),
})


@automation.register_action(ACTION_JOG, GrblJogAction, GRBL_JOG_SCHEMA, synchronous=True)
async def grbl_jog_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_GRBL_ID])
    x = await cg.templatable(config[CONF_DX], args, cg.float_)
    cg.add(var.set_x(x))
    y = await cg.templatable(config[CONF_DY], args, cg.float_)
    cg.add(var.set_y(y))
    z = await cg.templatable(config[CONF_DZ], args, cg.float_)
    cg.add(var.set_z(z))
    speed = await cg.templatable(config[CONF_SPEED], args, cg.int_)
    cg.add(var.set_speed(speed))
    return var


# grbl.set_home action

GrblSetHomeAction = ns.class_("GrblSetHomeAction", automation.Action, cg.Parented.template(Grbl))

GRBL_SET_HOME_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Optional(CONF_XY, default=True): cv.templatable(cv.boolean),
    cv.Optional(CONF_Z, default=True): cv.templatable(cv.boolean),
    cv.Optional(CONF_COORDS, default="G54"): cv.templatable(cv.enum(GRBL_COORDS, upper=True)),
})


@automation.register_action(ACTION_SET_HOME, GrblSetHomeAction, GRBL_SET_HOME_SCHEMA, synchronous=True)
async def grbl_set_home_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_GRBL_ID])
    xy = await cg.templatable(config[CONF_XY], args, cg.bool_)
    cg.add(var.set_xy(xy))
    z = await cg.templatable(config[CONF_Z], args, cg.bool_)
    cg.add(var.set_z(z))
    coords = await cg.templatable(config[CONF_COORDS], args, GrblCoords)
    cg.add(var.set_coords(coords))
    return var


# grbl.probe_z action

GrblProbeZAction = ns.class_("GrblProbeZAction", automation.Action, cg.Parented.template(Grbl))

GRBL_PROBE_Z_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Optional(CONF_DISTANCE, default=CONF_AUTO): cv.templatable(float_or_auto),
    cv.Optional(CONF_SEEK_RATE, default=100.0): cv.templatable(cv.float_),
    cv.Optional(CONF_FEED_RATE, default=10.0): cv.templatable(cv.float_),
    cv.Optional(CONF_OFFSET, default=0.0): cv.templatable(cv.float_),
    cv.Optional(CONF_RETRACT, default=5.0): cv.templatable(cv.float_),
    cv.Optional(CONF_COORDS, default="G54"): cv.templatable(cv.enum(GRBL_COORDS, upper=True)),
})


@automation.register_action("grbl.probe_z", GrblProbeZAction, GRBL_PROBE_Z_SCHEMA, synchronous=True)
async def grbl_probe_z_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_GRBL_ID])
    distance = await cg.templatable(config[CONF_DISTANCE], args, cg.float_)
    cg.add(var.set_distance(distance))
    seek_rate = await cg.templatable(config[CONF_SEEK_RATE], args, cg.float_)
    cg.add(var.set_seek_rate(seek_rate))
    feed_rate = await cg.templatable(config[CONF_FEED_RATE], args, cg.float_)
    cg.add(var.set_feed_rate(feed_rate))
    offset = await cg.templatable(config[CONF_OFFSET], args, cg.float_)
    cg.add(var.set_offset(offset))
    retract = await cg.templatable(config[CONF_RETRACT], args, cg.float_)
    cg.add(var.set_retract(retract))
    coords = await cg.templatable(config[CONF_COORDS], args, GrblCoords)
    cg.add(var.set_coords(coords))
    return var

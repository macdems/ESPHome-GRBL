from logging import config

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PORT

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

    @automation.register_action(
        action_name,
        cls,
        GRBL_SIMPLE_ACTION_SCHEMA,
    )
    async def action_to_code(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg)
        await cg.register_parented(var, config[CONF_GRBL_ID])
        return var

    return cls


# grbl.cancel_jog, grbl.reset and grbl.release_state actions

GrblCancelJogAction = register_grbl_simple_action(ACTION_CANCEL_JOG, "GrblCancelJogAction")
GrblSendResetAction = register_grbl_simple_action(ACTION_RESET, "GrblSendResetAction")
GrblReleaseStateAction = register_grbl_simple_action(ACTION_RELEASE_STATE, "GrblReleaseStateAction")

# grbl.send_command action

GrblSendCommandAction = ns.class_("GrblSendCommandAction", automation.Action, cg.Parented.template(Grbl))

GRBL_SEND_COMMAND_SCHEMA = cv.maybe_simple_value(
    cv.Schema({
        cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    }),
    key=CONF_COMMAND,
)


@automation.register_action(ACTION_SEND_COMMAND, GrblSendCommandAction, GRBL_SEND_COMMAND_SCHEMA)
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


@automation.register_action(ACTION_JOG, GrblJogAction, GRBL_JOG_SCHEMA)
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


# grbl.set_home

GrblSetHomeAction = ns.class_("GrblSetHomeAction", automation.Action, cg.Parented.template(Grbl))

GRBL_SET_HOME_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Optional(CONF_XY, default=True): cv.boolean,
    cv.Optional(CONF_Z, default=True): cv.boolean,
})


@automation.register_action(ACTION_SET_HOME, GrblSetHomeAction, GRBL_SET_HOME_SCHEMA)
async def grbl_set_home_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_GRBL_ID])
    xy = config[CONF_XY]
    cg.add(var.set_xy(xy))
    z = config[CONF_Z]
    cg.add(var.set_z(z))
    return var

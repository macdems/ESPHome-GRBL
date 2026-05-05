import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_DEVICE_CLASS, CONF_ID, DEVICE_CLASS_CONNECTIVITY

from . import Grbl, ValidateAxisDependency, grbl_param, ns
from .const import *

DEPENDENCIES = ['grbl']

# bs = cg.esphome_ns.namespace('binary_sensor')

GrblBinarySensor = ns.class_("GrblBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema().extend({
        cv.GenerateID(CONF_ID): cv.declare_id(binary_sensor.BinarySensor), \
        cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
        cv.Required(CONF_ENTITY_PARAM): cv.Any(
            cv.enum({
                CONF_CLIENT_CONNECTED: CONF_CLIENT_CONNECTED,
                CONF_LIMIT: CONF_LIMIT,
                CONF_PROBE: CONF_PROBE
            }, lower=True),
            grbl_param,
        ),
        cv.Optional(CONF_AXIS): cv.enum({
            CONF_X: 'X',
            CONF_Y: 'Y',
            CONF_Z: 'Z'
        }, lower=True)
    }),
    ValidateAxisDependency(CONF_LIMIT)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRBL_ID])
    param = config[CONF_ENTITY_PARAM]

    if param == CONF_CLIENT_CONNECTED:
        if not CONF_DEVICE_CLASS in config:
            config[CONF_DEVICE_CLASS] = DEVICE_CLASS_CONNECTIVITY
        var = await binary_sensor.new_binary_sensor(config)
        cg.add(parent.register_connection_sensor(var))

    elif param == CONF_LIMIT:
        axis = config[CONF_AXIS]
        var = await binary_sensor.new_binary_sensor(config)
        cg.add(parent.register_limit_sensor(axis, var))

    elif param == CONF_PROBE:
        var = await binary_sensor.new_binary_sensor(config)
        cg.add(parent.register_probe_sensor(var))

    else:
        var = cg.new_Pvariable(config[CONF_ID], GrblBinarySensor)
        await binary_sensor.register_binary_sensor(var, config)
        await cg.register_component(var, config)
        cg.add(var.set_parent(parent))
        cg.add(var.set_setting(param))
        cg.add(parent.register_listener(var))

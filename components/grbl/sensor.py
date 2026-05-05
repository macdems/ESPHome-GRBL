import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

from . import Grbl, ValidateAxisDependency, grbl_param, ns
from .const import *

GrblSensor = ns.class_("GrblSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema().extend({
        cv.GenerateID(CONF_ID): cv.declare_id(sensor.Sensor), \
        cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
        cv.Required(CONF_ENTITY_PARAM): cv.Any(
            cv.enum({
                CONF_MPOS: CONF_MPOS,
                CONF_WPOS: CONF_WPOS,
                CONF_WCO: CONF_WCO,
                CONF_FEED_RATE: CONF_FEED_RATE,
                CONF_SPINDLE_SPEED: CONF_SPINDLE_SPEED,
            }, lower=True),
            grbl_param,
        ),
        cv.Optional(CONF_AXIS): cv.enum({
            CONF_X: 'X',
            CONF_Y: 'Y',
            CONF_Z: 'Z'
        }, lower=True),
    }),
    ValidateAxisDependency(CONF_MPOS, CONF_WPOS, CONF_WCO)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRBL_ID])
    param = config[CONF_ENTITY_PARAM]

    if param == CONF_MPOS:
        axis = config[CONF_AXIS]
        var = await sensor.new_sensor(config)
        cg.add(parent.register_mpos_sensor(axis, var))
    elif param == CONF_WPOS:
        axis = config[CONF_AXIS]
        var = await sensor.new_sensor(config)
        cg.add(parent.register_wpos_sensor(axis, var))
    elif param == CONF_WCO:
        axis = config[CONF_AXIS]
        var = await sensor.new_sensor(config)
        cg.add(parent.register_wco_sensor(axis, var))
    elif param == CONF_FEED_RATE:
        var = await sensor.new_sensor(config)
        cg.add(parent.register_fs_feed_sensor(var))
    elif param == CONF_SPINDLE_SPEED:
        var = await sensor.new_sensor(config)
        cg.add(parent.register_fs_speed_sensor(var))

    else:
        var = cg.new_Pvariable(config[cv.CONF_ID], GrblSensor)
        await sensor.register_sensor(var, config)
        await cg.register_component(var, config)
        cg.add(var.set_parent(parent))
        cg.add(var.set_setting(param))
        cg.add(parent.register_listener(var))

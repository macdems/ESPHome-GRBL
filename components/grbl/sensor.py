import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from . import Grbl, grbl_param, ns
from .const import CONF_ENTITY_PARAM, CONF_GRBL_ID

GrblSensor = ns.class_("GrblSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(GrblSensor).extend({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Required(CONF_ENTITY_PARAM): grbl_param,
})


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_GRBL_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_setting(config[CONF_ENTITY_PARAM]))
    cg.add(parent.register_listener(var))

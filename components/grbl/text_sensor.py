import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import Grbl
from .const import *

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend({
        cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
        cv.Required(CONF_ENTITY_PARAM): cv.enum({
            CONF_STATE: CONF_STATE,
    })
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRBL_ID])
    param = config[CONF_ENTITY_PARAM]

    if param == CONF_STATE:
        var = await text_sensor.new_text_sensor(config)
        cg.add(parent.register_state_text_sensor(var))

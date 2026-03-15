import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP

from . import Grbl, grbl_param, ns
from .const import CONF_ENTITY_PARAM, CONF_GRBL_ID

GrblNumber = ns.class_("GrblNumber", number.Number, cg.PollingComponent)

CONFIG_SCHEMA = number.number_schema(GrblNumber).extend({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Required(CONF_ENTITY_PARAM): grbl_param,
    cv.Required(CONF_MIN_VALUE): cv.float_,
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Required(CONF_STEP): cv.float_,
})


async def to_code(config):
    var = await number.new_number(
        config, min_value=config[CONF_MIN_VALUE], max_value=config[CONF_MAX_VALUE], step=config[CONF_STEP]
    )
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_GRBL_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_setting(config[CONF_ENTITY_PARAM]))
    cg.add(parent.register_listener(var))

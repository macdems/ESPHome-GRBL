import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import Grbl, grbl_param, ns
from .const import CONF_ENTITY_PARAM, CONF_GRBL_ID

GrblSwitch = ns.class_("GrblSwitch", switch.Switch, cg.PollingComponent)

CONFIG_SCHEMA = switch.switch_schema(GrblSwitch).extend({
    cv.GenerateID(CONF_GRBL_ID): cv.use_id(Grbl),
    cv.Required(CONF_ENTITY_PARAM): grbl_param,
})


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_GRBL_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_setting(config[CONF_ENTITY_PARAM]))
    cg.add(parent.register_listener(var))

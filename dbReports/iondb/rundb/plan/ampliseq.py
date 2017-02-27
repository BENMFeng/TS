# Copyright (C) 2013 Ion Torrent Systems, Inc. All Rights Reserved

from iondb.rundb.models import Plugin
import json
import os
import sys
# This is the latest version of the TS for which AmpliSeq.com has needed
# modification to accomodate.  For example, we could leave this string
# "3.6" during the TS 4.0 release if the TS 4.0 release is compatible with
# the AmpliSeq output for the 3.6 TS in every way.
# Note: this is unlikely for most releases as a change to the Variant Caller,
# the Plan schema, or the BED publisher might necessitate changes here.
CURRENT_VERSION = "5.2"


def setup_vc_config_36(plan):
    vc_config = plan.pop("variant_caller", None)
    plan["selectedPlugins"] = {}
    # If there's no VC config, we'll just skip the entire plugin configuration
    if vc_config is not None:
        name = "variantCaller"
        plugin = Plugin.objects.filter(name=name, active=True).order_by('-version')[0]

        # Add the VC config to the required frame JSON for the plan
        plan["selectedPlugins"] = {
            "variantCaller": {
                "features": [],
                "id": plugin.id,
                "name": "variantCaller",
                "userInput": vc_config,
                "ampliSeqVariantCallerConfig": vc_config,
                "version": plugin.version,
            }
        }
    return plan

def setup_other_plugin_config(plan):
    plugin_config = plan.pop("plugins", None)
    inValidPlugins = validatePlugins(plugin_config)

    plugins = {}
    if not inValidPlugins:
        for pluginName, paramFile in plugin_config.iteritems():
            plugin_config = plan.pop(pluginName, None)
            # skip the variant caller since VC has already been configured by setup_vc_config
            # Proceed with other plugin configuration
            if pluginName and pluginName != 'variantCaller':
                plugin = Plugin.objects.filter(name=pluginName, active=True).order_by('-version')[0]
                # Add the other plugin config to the required frame JSON for the plan
                pluginDict = {
                    "features": [],
                    "id": plugin.id,
                    "name": plugin.name,
                    "userInput": plugin_config,
                    "version": plugin.version
                }
                plugins[plugin.name] = pluginDict

        plan["selectedPlugins"].update(plugins)
    else:
        inValidPlugins_str = (', '.join(inValidPlugins))
        print ("ERROR: Invalid plugins found in the Plan.json: (%s)" % inValidPlugins_str)
        sys.exit(1)
    return plan


def legacy_plan_handler(data):
    """This is crudely supported.
    It will successfully import the BED file.
    """
    data["plan"] = dict(data)
    return data


def config_choice_handler_4_0(data, meta, config_choices):
    choice = meta.get("choice", None)
    keys = config_choices.keys()

    if len(keys) == 1 or choice not in keys:
        choice = sorted(keys)[0]
        #if user selects the wrong instrument type, TS should send validation error(TS-12754)
        #meta["choice"] = choice

    plan = config_choices[choice]

    if "runType" in plan:
        if plan["runType"] == "AMPS_DNA":
            plan["runType"] = "AMPS"

    plan = setup_vc_config_36(plan)

    data["plan"] = plan
    data["configuration_choices"] = keys
    return data, meta


def config_choice_handler_5_2(data, meta, config_choices, ampSeq_path=None):
    (plan, config_instrument_data) = get_choice_specific_planData(meta, config_choices)

    plan = setup_vc_config_36(plan)

    if "plugins" in plan and not ampSeq_path:
        plan = setup_other_plugin_config(plan)

    data["plan"] = plan
    data["configuration_choices"] = config_instrument_data
    return data, meta


def config_choice_handler_5_0(data, meta, config_choices):
    choice = meta.get("choice", None)
    if choice == "p1":
        choice = "proton"
    keys = config_choices.keys()
    if len(keys) == 1 or choice not in keys:
        existing_choice = sorted(keys)[0]
        config_choices[choice] = config_choices[existing_choice]
    plan = config_choices[choice]

    if "runType" in plan:
        if plan["runType"] == "AMPS_DNA":
            plan["runType"] = "AMPS"

    plan = setup_vc_config_36(plan)

    data["plan"] = plan
    data["configuration_choices"] = keys
    return data, meta


def plan_handler_5_2(data, meta, arg_path=None):
    config_choices = data["plan"]["5.2"]["configuration_choices"]

    if not arg_path:
        return config_choice_handler_5_2(data, meta, config_choices, ampSeq_path=True)

    for key, value in config_choices.iteritems():
        if "plugins" in config_choices[key]:
            allPluginPath = config_choices[key]['plugins']
            for pluginName, paramFile in allPluginPath.iteritems():
                specificPluginData = {}
                if pluginName:
                    pluginName = pluginName.strip()
                    if pluginName == "variantCaller":
                        pluginName = "variant_caller"
                    if paramFile:
                        specificPluginData = json.load(open(os.path.join(arg_path, paramFile)))
                        config_choices[key][pluginName] = specificPluginData
                    else:
                        config_choices[key][pluginName] = {}
    return config_choice_handler_5_2(data, meta, config_choices)


def plan_handler_5_0(data, meta):
    config_choices = data["plan"]["5.0"]["configuration_choices"]
    return config_choice_handler_5_0(data, meta, config_choices)


def plan_handler_4_6(data, meta):
    config_choices = data["plan"]["4.6"]["configuration_choices"]
    return config_choice_handler_4_0(data, meta, config_choices)


def plan_handler_4_4(data, meta):
    """
    current plan handler
    """
    config_choices = data["plan"]["4.4"]["configuration_choices"]
    return config_choice_handler_4_0(data, meta, config_choices)


def plan_handler_4_2(data, meta):
    config_choices = data["plan"]["4.2"]["configuration_choices"]
    return config_choice_handler_4_0(data, meta, config_choices)


def plan_handler_4_0(data, meta):
    config_choices = data["plan"]["4.0"]["configuration_choices"]
    return config_choice_handler_4_0(data, meta, config_choices)


def plan_handler_3_6(data, meta):
    """This is the original version that was versioned in this"""
    plan = data["plan"]["3.6"]

    if "runType" in plan:
        if plan["runType"] == "AMPS_DNA":
            plan["runType"] = "AMPS"
    elif "pipeline" in data:
        if data["pipeline"] == "RNA":
            plan["runType"] = "AMPS_RNA"
        else:
            plan["runType"] = "AMPS"

    plan = setup_vc_config_36(plan)

    data["plan"] = plan
    data["configuration_choices"] = []
    return data, meta


# This maps the logical version numbers in the ampliseq plan json
# into their individual parsing functions
# each such function must be checked each release for compatibility
version_plan_handlers = {
    "5.2": plan_handler_5_2,
    "5.0": plan_handler_5_0,
    "4.6": plan_handler_4_6,
    "4.4": plan_handler_4_4,
    "4.2": plan_handler_4_2,
    "4.0": plan_handler_4_0,
    "3.6": plan_handler_3_6,
}


def handle_versioned_plans(data, meta=None, arg_path=None):
    """This validates and parses the plan.json JSON into an
    object meant to be read by the system in the current version.
    """
    if meta is None:
        meta = {}
    # This is the very first iteration of AmpliSeq zip exports to be used
    # by the TS and it might not need to be supported at all
    if "plan" not in data:
        return "legacy", legacy_plan_handler(data), meta
    # The plan is empty or null
    elif not data['plan']:
        data["plan"] = {}
        return "unplanned", data, meta
    # This is the version we want to find, the version for *this* TS version
    # even if later versions are available in the JSON
    elif CURRENT_VERSION in data["plan"]:
        version_plan_handlers_args = [data, meta]
        config_choices = data["plan"][CURRENT_VERSION]["configuration_choices"]
        # Check if the plan Data is in new format or old
        # if "plugins key" exists in the plan, send the plugin path arg to extract the param file configuration.
        (checkPlan2Json, config_instrument_data) = get_choice_specific_planData(meta, config_choices)
        if "plugins" in checkPlan2Json:
            version_plan_handlers_args.append(arg_path)
        data, meta = version_plan_handlers[CURRENT_VERSION](*version_plan_handlers_args)

        return CURRENT_VERSION, data, meta
    # If the current version isn't in there, it's because the zip is older
    # than the current version; however, it's possible that we know how
    # to handle archives from that older version for this TS version
    else:
        max_version = max(data["plan"].keys())
        if max_version in version_plan_handlers:
            data, meta = version_plan_handlers[max_version](data, meta)
            return max_version, data, meta
    return None, None, None


def get_choice_specific_planData(meta, config_choices):
    choice = meta.get("choice", None)
    if choice == "p1":
        choice = "proton"
    config_instrument_data = config_choices.keys()

    if len(config_instrument_data) == 1 or choice not in config_instrument_data:
        choice = sorted(config_instrument_data)[0]
        #if user selects the wrong instrument type, TS should send validation error(TS-12754)
        #meta["choice"] = choice
    plan = config_choices[choice]
    available_choice = []
    for available in config_instrument_data:
        available = str(available.upper())
        if available in ['520', '521', '530', '540']:
            available = "S5 Chip: " + available
        available_choice.append(available)
    plan["available_choice"] = available_choice
    if "runType" in plan:
        if plan["runType"] == "AMPS_DNA":
            plan["runType"] = "AMPS"

    return (plan, config_instrument_data)


def validatePlugins(plugin_config):
    inValidPlugins = []
    for pluginName, paramFile in plugin_config.iteritems():
        qs = Plugin.objects.filter(name=pluginName, active=True).order_by('id')
        if qs.count() == 0:
            inValidPlugins.append(pluginName)

    return inValidPlugins

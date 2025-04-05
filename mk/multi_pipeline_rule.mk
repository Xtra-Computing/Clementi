

TEMPLATE_PATH = app/${TEMPLATE}
TEMPLATE_MK = ${TEMPLATE_PATH}/kernel.mk
TEMPLATE_CFG = ${TEMPLATE_PATH}/kernel.cfg

include ${TEMPLATE_MK}

CXXFLAGS += -DACC_NUM=${NUMBER_OF_PIPELINE}

.PHONY: ${DEFAULT_CFG}

${DEFAULT_CFG}: app/$(APP)/kernel.mk
	../script/duplicte_kernel.py --np ${NUMBER_OF_PIPELINE} --input ${TEMPLATE_CFG}  --output $@
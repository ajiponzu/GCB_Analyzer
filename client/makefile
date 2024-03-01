#
#  makefile for GCB client.
#
# Fri Dec  1 01:59:34 2023 JST
#

all:  gcb suzuka8HR JSB EVAL

gcb:
	python client.py resource/gcb/gcb_0_*.json

#
#
suzuka8HR:  HRC33
HRC33: lap28 lap29
lap28:
	python client.py resource/suzuka8HR/HRC33/HRC33_lap28_*.json
lap29:
	python client.py resource/suzuka8HR/HRC33/HRC33_lap29_*.json

#
#
JSB:  JSB_sone JSB_kitani
JSB_sone:	
	for file in resource/suzukaJSB/sone/*.json; do \
           python client.py  "$$file"; \
	done

JSB_kitani:
	for file in resource/suzukaJSB/kitani/*.json; do \
           python client.py  "$$file"; \
	done

#
#
EVAL: eval0 eval1 eval2 eval3 eval4 eval5
eval0:
	for file in resource/eval0/*.json; do \
           python client.py  "$$file"; \
	done
eval1:
	for file in resource/eval1/*.json; do \
           python client.py  "$$file"; \
	done
eval2:
	for file in resource/eval2/*.json; do \
           python client.py  "$$file"; \
	done
eval3:
	for file in resource/eval3/*.json; do \
           python client.py  "$$file"; \
	done

eval4:
	for file in resource/eval4/*.json; do \
           python client.py  "$$file"; \
	done
eval5:
	for file in resource/eval5/*.json; do \
           python client.py  "$$file"; \
	done

#
#  Build resource files to ./resource/*/*
#
INPUTDIR = "/Users/sone/dropbox/Public"
BUILD:   build_eval0 build_eval1 build_eval2 build_eval3 build_eval4 build_eval5  build_suzuka8HR build_suzukaJSB

#
build_eval0:
	python resource_builder.py -i ${INPUTDIR} resource/eval0.json
build_eval1:
	python resource_builder.py -i ${INPUTDIR} resource/eval1.json
build_eval2:
	python resource_builder.py -i ${INPUTDIR} resource/eval2.json
build_eval3:
	python resource_builder.py -i ${INPUTDIR} resource/eval3.json
build_eval4:
	python resource_builder.py -i ${INPUTDIR} resource/eval4.json
build_eval5:
	python resource_builder.py -i ${INPUTDIR} resource/eval5_i15.json
	python resource_builder.py -i ${INPUTDIR} resource/eval5_gp.json
	python resource_builder.py -i ${INPUTDIR} resource/eval5_ins.json
#
build_suzuka8HR:
	python resource_builder.py -i ${INPUTDIR} resource/suzuka8HR.json

build_suzukaJSB:
	python resource_builder.py -i ${INPUTDIR} resource/suzukaJSB_sone.json
	python resource_builder.py -i ${INPUTDIR} resource/suzukaJSB_kitani.json

#
# Clean up
#
clean: clean_temp clean_result
clean_temp:
	rm -f temp/*

clean_result:
	rm -f result/*
#
#
clean_resource:
	rm -rf resource/{eval?,suzuka8HR,suzukaJSB}

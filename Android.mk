LOCAL_PATH := $(call my-dir)

#
# Add here all products which are using ALSA and asound.conf
# to establish the audio routing
#
LEGACY_ROUTING_PRODUCTS := \
        mfld_pr2 \
        mfld_gi \
        yukkabeach \
        mfld_dv10 \
        redridge \
        redridge3G \
        salitpa \
        ctp_pr1 \
        ctp_nomodem \
        victoriabay
#
# Add here all products which are using enhanced Audio Route Manager
# and Parameter Framework to establish the audio routing
# These products have SSP port controlled by IA directly
#
IA_CONTROLLED_SSP_ROUTING_PRODUCTS := \
#        ctp_XXX \
#
# Add here all products which are
#   - LPE centric
#   - using "simplified enhanced" Audio Route Manager
# and Parameter Framework to establish the audio routing
#
LPE_CENTRIC_ROUTING_PRODUCTS := \
        mrfl_vp \
        mrfl_hvp \
        mrfl_sle

ifneq ($(findstring $(TARGET_PRODUCT),$(LEGACY_ROUTING_PRODUCTS)),)

$(info platform using ALSA LIB for routing)
include $(LOCAL_PATH)/audio_hw_legacy/Android.mk

else ifneq ($(findstring $(TARGET_PRODUCT),$(IA_CONTROLLED_SSP_ROUTING_PRODUCTS)),)

$(info  For shared audio bus architecture platforms using RouteManager/PFW for routing)
include $(LOCAL_PATH)/audio_hw_ctp_dev/Android.mk

else ifneq ($(findstring $(TARGET_PRODUCT),$(LPE_CENTRIC_ROUTING_PRODUCTS)),)

$(info For LPE centric platforms using RouteManager/PFW for routing)
include $(LOCAL_PATH)/audio_hw_lpe_centric/Android.mk

else

$(info TARGET_PRODUCT=$(TARGET_PRODUCT) not identified, add it into this Android.mk!!!)

endif # ifneq ($(findstring $(TARGET_PRODUCT),$(LEGACY_ROUTING_PRODUCTS)),)

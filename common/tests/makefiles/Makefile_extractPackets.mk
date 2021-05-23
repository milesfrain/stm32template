COMPONENT_NAME=extractPackets

SRC_FILES = \
  $(PROJECT_SRC_DIR)/packet_utils.cpp \
  $(PROJECT_SRC_DIR)/software_crc.cpp \

TEST_SRC_FILES = \
  $(UNITTEST_SRC_DIR)/test_extractPackets.cpp

include $(CPPUTEST_MAKFILE_INFRA)

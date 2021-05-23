COMPONENT_NAME=crc

SRC_FILES = \
  $(PROJECT_SRC_DIR)/software_crc.cpp \

TEST_SRC_FILES = \
  $(UNITTEST_SRC_DIR)/test_software_crc.cpp

include $(CPPUTEST_MAKFILE_INFRA)

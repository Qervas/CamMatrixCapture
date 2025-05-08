# Image Processing module
set(IMAGE_PROCESSING_SOURCES
    filter.cpp
    filter_base.cpp
    filter_registry.cpp
    filters/grayscale_filter.cpp
    filters/invert_filter.cpp
    filters/blur_filter.cpp
    filters/brightness_contrast_filter.cpp
    filters/sepia_filter.cpp
    filters/saturation_filter.cpp
    filters/sharpen_filter.cpp
    filters/threshold_filter.cpp
    filters/edge_detection_filter.cpp
    filters/vignette_filter.cpp
)

set(IMAGE_PROCESSING_HEADERS
    filter.hpp
    filter_base.hpp
    filter_registry.hpp
    filters/grayscale_filter.hpp
    filters/invert_filter.hpp
    filters/blur_filter.hpp
    filters/brightness_contrast_filter.hpp
    filters/sepia_filter.hpp
    filters/saturation_filter.hpp
    filters/sharpen_filter.hpp
    filters/threshold_filter.hpp
    filters/edge_detection_filter.hpp
    filters/vignette_filter.hpp
)

# Add the sources to the parent scope variable
set(CORE_SOURCES ${CORE_SOURCES} ${IMAGE_PROCESSING_SOURCES} PARENT_SCOPE)
set(CORE_HEADERS ${CORE_HEADERS} ${IMAGE_PROCESSING_HEADERS} PARENT_SCOPE) 
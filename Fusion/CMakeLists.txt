
####application folder
set(FUSION_APPLICATION_SRCS
	${SRC_PATH}/applications/Application.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_APPLICATION_SRCS})

set(FUSION_APPLICATION_HDRS
	${SRC_PATH}/applications/Application.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_APPLICATION_HDRS})

####gui folder
set(FUSION_GUI_SRCS
	${SRC_PATH}/gui/CreateCaseDialog.cpp
	${SRC_PATH}/gui/OpenCaseDialog.cpp
	${SRC_PATH}/gui/GuiMessage.cpp
	${SRC_PATH}/gui/QtGuiStyle.cpp
	${SRC_PATH}/gui/BaseMainWindow.cpp
	${SRC_PATH}/gui/AboutDialog.cpp
	${SRC_PATH}/gui/LogViewerDialog.cpp
	${SRC_PATH}/gui/EncryptPasswordDialog.cpp
	${SRC_PATH}/gui/ReviewFileDialog.cpp
	${SRC_PATH}/gui/ChooseLanguageDialog.cpp
	${SRC_PATH}/gui/FactoryMenu.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_GUI_SRCS})

set(FUSION_GUI_HDRS
	${SRC_PATH}/gui/CreateCaseDialog.h
	${SRC_PATH}/gui/OpenCaseDialog.h
	${SRC_PATH}/gui/GuiMessage.h
	${SRC_PATH}/gui/QtGuiStyle.h
	${SRC_PATH}/gui/BaseMainWindow.h
	${SRC_PATH}/gui/AboutDialog.h
	${SRC_PATH}/gui/LogViewerDialog.h
	${SRC_PATH}/gui/EncryptPasswordDialog.h
	${SRC_PATH}/gui/ReviewFileDialog.h
	${SRC_PATH}/gui/ChooseLanguageDialog.h
	${SRC_PATH}/gui/FactoryMenu.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_GUI_HDRS})

####imaging folder 
set(FUSION_IMAGING_SRCS
	${SRC_PATH}/imaging/ImageStack.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_IMAGING_SRCS})

set(FUSION_IMAGING_HDRS
	${SRC_PATH}/imaging/ImageStack.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_IMAGING_HDRS})

####surgery folder
set(FUSION_SURGERY_SRCS
	${SRC_PATH}/surgery/BaseSurgeryController.cpp
	${SRC_PATH}/surgery/BaseSurgery.cpp
	${SRC_PATH}/surgery/SurgeryCase.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_SURGERY_SRCS})

set(FUSION_SURGERY_HDRS
	${SRC_PATH}/surgery/BaseSurgeryController.h
	${SRC_PATH}/surgery/BaseSurgery.h
	${SRC_PATH}/surgery/SurgeryCase.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_SURGERY_HDRS})

####utility folder
set (FUSION_UTILITY_SRCS
	${SRC_PATH}/utility/CommonClasses.cpp
	${SRC_PATH}/utility/UtlLogger.cpp
	${SRC_PATH}/utility/UtlMetaRecord.cpp
	${SRC_PATH}/utility/UtlThread.cpp
	${SRC_PATH}/utility/UroLicense.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_UTILITY_SRCS})

set (FUSION_UTILITY_HDRS
	${SRC_PATH}/utility/CommonClasses.h
	${SRC_PATH}/utility/Constants.h
	${SRC_PATH}/utility/UtlLogger.h
	${SRC_PATH}/utility/UtlMetaRecord.h
	${SRC_PATH}/utility/UtlThread.h
	${SRC_PATH}/utility/UroLicense.h
	${SRC_PATH}/utility/Version.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_UTILITY_HDRS})


####visualization folder
set(FUSION_VISUALIZATION_SRCS
	${SRC_PATH}/visualization/BaseVisualEngine.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_VISUALIZATION_SRCS})

set(FUSION_VISUALIZATION_HDRS
	${SRC_PATH}/visualization/BaseVisualEngine.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_VISUALIZATION_HDRS})

set(FUSION_VISUALIZATION_CONTEXTS_SRCS
	${SRC_PATH}/visualization/Contexts/ImageContext.cpp
	${SRC_PATH}/visualization/Contexts/ModellingTransversalImageContext.cpp
	${SRC_PATH}/visualization/Contexts/SagCorImageContext.cpp
	${SRC_PATH}/visualization/Contexts/TransversalImageContext.cpp
	${SRC_PATH}/visualization/Contexts/VirtualContext.cpp	
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_VISUALIZATION_CONTEXTS_SRCS})

set(FUSION_VISUALIZATION_CONTEXTS_HDRS
	${SRC_PATH}/visualization/Contexts/ImageContext.h
	${SRC_PATH}/visualization/Contexts/ModellingTransversalImageContext.h
	${SRC_PATH}/visualization/Contexts/SagCorImageContext.h
	${SRC_PATH}/visualization/Contexts/TransversalImageContext.h
	${SRC_PATH}/visualization/Contexts/VirtualContext.h	
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_VISUALIZATION_CONTEXTS_HDRS})

set(FUSION_VISUALIZATION_DISPLAYOBJECTS_SRCS
	${SRC_PATH}/visualization/DisplayObjects/AnnotationDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/AnnotationScaleVTKDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/CurveDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/ModelDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/Label2DDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/Label3DDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/SubModelDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/SurfaceDisplayObject.cpp
	${SRC_PATH}/visualization/DisplayObjects/VolumeDisplayObject.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_VISUALIZATION_DISPLAYOBJECTS_SRCS})

set(FUSION_VISUALIZATION_DISPLAYOBJECTS_HDRS
	${SRC_PATH}/visualization/DisplayObjects/AnnotationDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/AnnotationScaleVTKDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/CurveDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/ModelDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/Label2DDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/Label3DDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/SubModelDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/SurfaceDisplayObject.h
	${SRC_PATH}/visualization/DisplayObjects/VolumeDisplayObject.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_VISUALIZATION_DISPLAYOBJECTS_HDRS})

set(FUSION_VISUALIZATION_INTERACTORSTYLES_SRCS
	${SRC_PATH}/visualization/InteractorStyles/CursorBoundResliceInteractorStyle.cpp
	${SRC_PATH}/visualization/InteractorStyles/ModelInteractorStyle.cpp
	${SRC_PATH}/visualization/InteractorStyles/SagittalModelInteractorStyle.cpp
	${SRC_PATH}/visualization/InteractorStyles/ResliceInteractorStyle.cpp
	${SRC_PATH}/visualization/InteractorStyles/VirtualImageInteractorStyle.cpp
	${SRC_PATH}/visualization/InteractorStyles/ZoomPanImageInteractorStyle.cpp
	${SRC_PATH}/visualization/InteractorStyles/DistanceMeasurementImageInteractorStyle.cpp
)
list(APPEND ${PROJECT_NAME}_SRCS ${FUSION_VISUALIZATION_INTERACTORSTYLES_SRCS})

set(FUSION_VISUALIZATION_INTERACTORSTYLES_HDRS
	${SRC_PATH}/visualization/InteractorStyles/CursorBoundResliceInteractorStyle.h
	${SRC_PATH}/visualization/InteractorStyles/ModelInteractorStyle.h
	${SRC_PATH}/visualization/InteractorStyles/SagittalModelInteractorStyle.h
	${SRC_PATH}/visualization/InteractorStyles/ResliceInteractorStyle.h
	${SRC_PATH}/visualization/InteractorStyles/VirtualImageInteractorStyle.h
	${SRC_PATH}/visualization/InteractorStyles/ZoomPanImageInteractorStyle.h
	${SRC_PATH}/visualization/InteractorStyles/DistanceMeasurementImageInteractorStyle.h
)
list(APPEND ${PROJECT_NAME}_HDRS ${FUSION_VISUALIZATION_INTERACTORSTYLES_HDRS})


include_directories(${SRC_PATH}/applications)
include_directories(${SRC_PATH}/gui)
include_directories(${SRC_PATH}/imaging)
include_directories(${SRC_PATH}/surgery)
include_directories(${SRC_PATH}/utility)
include_directories(${SRC_PATH}/visualization)
include_directories(${SRC_PATH}/visualization/Contexts)
include_directories(${SRC_PATH}/visualization/DisplayObjects)
include_directories(${SRC_PATH}/visualization/InteractorStyles)


source_group("applications" FILES ${FUSION_APPLICATION_SRCS} ${FUSION_APPLICATION_HDRS})
source_group("gui" FILES ${FUSION_GUI_SRCS} ${FUSION_GUI_HDRS})
source_group("imaging" FILES ${FUSION_IMAGING_SRCS} ${FUSION_IMAGING_HDRS})
source_group("surgery" FILES ${FUSION_SURGERY_SRCS} ${FUSION_SURGERY_HDRS})
source_group("utility" FILES ${FUSION_UTILITY_SRCS} ${FUSION_UTILITY_HDRS})
source_group("visualization" FILES ${FUSION_VISUALIZATION_SRCS} ${FUSION_VISUALIZATION_HDRS})
source_group("visualization\\Contexts" FILES ${FUSION_VISUALIZATION_CONTEXTS_SRCS} ${FUSION_VISUALIZATION_CONTEXTS_HDRS})
source_group("visualization\\DisplayObjects" FILES ${FUSION_VISUALIZATION_DISPLAYOBJECTS_SRCS} ${FUSION_VISUALIZATION_DISPLAYOBJECTS_HDRS})
source_group("visualization\\InteractorSytles" FILES ${FUSION_VISUALIZATION_INTERACTORSTYLES_SRCS} ${FUSION_VISUALIZATION_INTERACTORSTYLES_HDRS})

AddDirectory(${SRC_PATH}/applications/Fusion)
AddDirectory(${SRC_PATH}/inurbs)
AddDirectory(${SRC_PATH}/modelling)
AddDirectory(${SRC_PATH}/qvtk)
AddDirectory(${SRC_PATH}/visualization/DrawObjects)
AddDirectory(${SRC_PATH}/pdp)
AddDirectory(${SRC_PATH}/translations)
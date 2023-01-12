#include <openxr/openxr.h>
#include "spdlog/spdlog.h"

namespace Side {
const int LEFT = 0;
const int RIGHT = 1;
const int COUNT = 2;
}  // namespace Side

struct InputState {
        XrActionSet actionSet{XR_NULL_HANDLE};
        XrAction grabAction{XR_NULL_HANDLE};
        XrAction poseAction{XR_NULL_HANDLE};
        XrAction vibrateAction{XR_NULL_HANDLE};
        XrAction thumbstickAction{XR_NULL_HANDLE};
        XrAction menuClickAction{XR_NULL_HANDLE};
        std::array<XrPath, Side::COUNT> handSubactionPath;
        std::array<XrSpace, Side::COUNT> handSpace;
        std::array<float, Side::COUNT> handScale = {{1.0f, 1.0f}};
        std::array<XrBool32, Side::COUNT> handActive;
};

struct Input_XR{

    InputState m_input;

    bool xr_result(XrInstance instance, XrResult result, const char* format, ...)
    {
        if (XR_SUCCEEDED(result))
            return true;

        char resultString[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, resultString);

        size_t len1 = strlen(format);
        size_t len2 = strlen(resultString) + 1;
        char *formatRes = new  char[len1 + len2 + 4];
        sprintf(formatRes, "%s [%s]\n", format, resultString);

        va_list args;
        va_start(args, format);
        vprintf(formatRes, args);
        va_end(args);

        delete [] formatRes;
        return false;
    }

    // create Actions, Poses, ActionSets, Paths, Action Spaces.
    void InitXrInput_MakeQueryable(XrInstance xrInstance, XrSession xrSession)
    {
        XrResult result;
        // Create an action set.
        {
            XrActionSetCreateInfo actionSetCreateInfo = { .type = XR_TYPE_ACTION_SET_CREATE_INFO, .next = NULL, .priority = 0};
            strcpy(actionSetCreateInfo.actionSetName, "gameplayActionSet");
            strcpy(actionSetCreateInfo.localizedActionSetName, "XR Gameplay Action Set");
            actionSetCreateInfo.priority = 0;
            result = xrCreateActionSet(xrInstance, &actionSetCreateInfo, &m_input.actionSet);
            if (!xr_result(xrInstance, result, "failed to create actionset"))
                return;
        }

        // Get the XrPath for the left and right hands - we will use them as subaction paths.
        xrStringToPath(xrInstance, "/user/hand/left", &m_input.handSubactionPath[Side::LEFT]); 
        xrStringToPath(xrInstance, "/user/hand/right", &m_input.handSubactionPath[Side::RIGHT]);

        // Create actions.
        {
            //[t] NOTE: if you don't specify "subactionPaths", 
            //then it means you bind the action for *any* controller, ie you trigger it without specifying controller.

            //[t] Create a grab input action used by both (all) hands.
            {
                XrActionCreateInfo actionInfo = {
                                                .type = XR_TYPE_ACTION_CREATE_INFO,
                                                .next = NULL,
                                                .actionType = XR_ACTION_TYPE_FLOAT_INPUT,
                                                .countSubactionPaths = uint32_t(m_input.handSubactionPath.size()),
                                                .subactionPaths = m_input.handSubactionPath.data()
                                                };
                strcpy(actionInfo.actionName, "grab_object_float");
                strcpy(actionInfo.localizedActionName, "Grab Object Float");

                result = xrCreateAction(m_input.actionSet, &actionInfo, &m_input.grabAction);
                if (!xr_result(xrInstance, result, "Failed to create the grab action"))
                    return;
            }

            //[t] Create an input action for getting the hand poses.
            {
                XrActionCreateInfo actionInfo = {
                                                .type = XR_TYPE_ACTION_CREATE_INFO,
                                                .next = NULL,
                                                .actionType = XR_ACTION_TYPE_POSE_INPUT,
                                                .countSubactionPaths = uint32_t(m_input.handSubactionPath.size()),
                                                .subactionPaths = m_input.handSubactionPath.data()
                                                };
                strcpy(actionInfo.actionName, "hand_m_input.poseAction");
                strcpy(actionInfo.localizedActionName, "Hand Pose Action");

                result = xrCreateAction(m_input.actionSet, &actionInfo, &m_input.poseAction);
                if (!xr_result(xrInstance, result, "Failed to create the hand pose action"))
                    return;
            }

            //[t] Create output actions for vibrating the controllers.
            {
                XrActionCreateInfo actionInfo = {
                                                .type = XR_TYPE_ACTION_CREATE_INFO,
                                                .next = NULL,
                                                .actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT,
                                                .countSubactionPaths = uint32_t(m_input.handSubactionPath.size()),
                                                .subactionPaths = m_input.handSubactionPath.data()
                                                };
                strcpy(actionInfo.actionName, "haptic_action");
                strcpy(actionInfo.localizedActionName, "Haptic Vibration Action");
                result = xrCreateAction(m_input.actionSet, &actionInfo, &m_input.vibrateAction);
                if (!xr_result(xrInstance, result, "Failed to create the haptic action"))
                    return;
            }
            
            //[t] Create a thumbstick axis action
            {
                XrActionCreateInfo actionInfo = 
                {
                    .type = XR_TYPE_ACTION_CREATE_INFO,
                    .next = NULL,
                    .actionType = XR_ACTION_TYPE_VECTOR2F_INPUT,
                    .countSubactionPaths = uint32_t(m_input.handSubactionPath.size()),
                    .subactionPaths = m_input.handSubactionPath.data()
                };
                strcpy(actionInfo.actionName, "thumbstick_action");
                strcpy(actionInfo.localizedActionName, "Thumbstick Axis");

                result = xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickAction);
                if (!xr_result(xrInstance, result, "failed to create thumbstick axis action"))
                    return;
            }

            //[t] Create a boolean action. TODO: maybe make an array here
            {
                XrActionCreateInfo actionInfo = 
                {
                .type = XR_TYPE_ACTION_CREATE_INFO,
                .next = NULL,
                .actionType = XR_ACTION_TYPE_BOOLEAN_INPUT,
                .countSubactionPaths = uint32_t(m_input.handSubactionPath.size()),
                .subactionPaths = m_input.handSubactionPath.data()
                };
                strcpy(actionInfo.actionName, "bool_0_action");
                strcpy(actionInfo.localizedActionName, "Boolean 0 Action");

                result = xrCreateAction(m_input.actionSet, &actionInfo, &m_input.menuClickAction);
                if (!xr_result(xrInstance, result, "failed to create menuClick action"))
                    return;
            }

        }

        //[t] boilerplate from khronos for getting binding paths
        std::array<XrPath, Side::COUNT> selectPath;
        std::array<XrPath, Side::COUNT> squeezeValuePath;
        std::array<XrPath, Side::COUNT> squeezeForcePath;
        std::array<XrPath, Side::COUNT> squeezeClickPath;
        std::array<XrPath, Side::COUNT> posePath;
        std::array<XrPath, Side::COUNT> hapticPath;
        std::array<XrPath, Side::COUNT> thumbstick_path;
        std::array<XrPath, Side::COUNT> menuClickPath;
        std::array<XrPath, Side::COUNT> bClickPath;
        std::array<XrPath, Side::COUNT> triggerValuePath;
        xrStringToPath(xrInstance, "/user/hand/left/input/select/click", &selectPath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/select/click", &selectPath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/squeeze/value", &squeezeValuePath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/squeeze/value", &squeezeValuePath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/squeeze/force", &squeezeForcePath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/squeeze/force", &squeezeForcePath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/squeeze/click", &squeezeClickPath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/grip/pose", &posePath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/grip/pose", &posePath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/output/haptic", &hapticPath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/output/haptic", &hapticPath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/thumbstick", &thumbstick_path[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/thumbstick", &thumbstick_path[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/menu/click", &menuClickPath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/menu/click", &menuClickPath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/b/click", &bClickPath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/b/click", &bClickPath[Side::RIGHT]);
        xrStringToPath(xrInstance, "/user/hand/left/input/trigger/value", &triggerValuePath[Side::LEFT]);
        xrStringToPath(xrInstance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side::RIGHT]);
        
        //[t] the bindings for multiple types of controllers. Also khronos boilerplate.
        // Suggest bindings for KHR Simple.
        {
            XrPath khrSimpleInteractionProfilePath;
            result = xrStringToPath(xrInstance, "/interaction_profiles/khr/simple_controller", &khrSimpleInteractionProfilePath);
            if (!xr_result(xrInstance, result, "Failed to get simple interaction profile"))
                return;

            std::vector<XrActionSuggestedBinding> bindings
            {{
                // Fall back to a click input for the grab action.
                {m_input.grabAction, selectPath[Side::LEFT]},
                {m_input.grabAction, selectPath[Side::RIGHT]},
                {m_input.poseAction, posePath[Side::LEFT]},
                {m_input.poseAction, posePath[Side::RIGHT]},
                {m_input.thumbstickAction, posePath[Side::LEFT]},
                {m_input.thumbstickAction, posePath[Side::RIGHT]},
                {m_input.menuClickAction, menuClickPath[Side::LEFT]},
                {m_input.menuClickAction, menuClickPath[Side::RIGHT]},
                {m_input.vibrateAction, hapticPath[Side::LEFT]},
                {m_input.vibrateAction, hapticPath[Side::RIGHT]}
            }};

            const XrInteractionProfileSuggestedBinding suggestedBindings = {
                .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                .next = NULL,
                .interactionProfile = khrSimpleInteractionProfilePath,
                .countSuggestedBindings = (uint32_t)bindings.size(),
                .suggestedBindings = bindings.data()
            };

            result = xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
            if (!xr_result(xrInstance, result, "Failed to suggest KHR Simple bindings"))
                return;
        }

        // Suggest bindings for the Oculus Touch.
        {
            XrPath oculusTouchInteractionProfilePath;
            result = xrStringToPath(xrInstance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath);
            if (!xr_result(xrInstance, result, "Failed to get Oculus Touch interaction profile"))
                return;

            std::vector<XrActionSuggestedBinding> bindings
            {{
                {m_input.grabAction, squeezeValuePath[Side::LEFT]},
                {m_input.grabAction, squeezeValuePath[Side::RIGHT]},
                {m_input.poseAction, posePath[Side::LEFT]},
                {m_input.poseAction, posePath[Side::RIGHT]},
                {m_input.thumbstickAction, posePath[Side::LEFT]},
                {m_input.thumbstickAction, posePath[Side::RIGHT]},
                {m_input.menuClickAction, menuClickPath[Side::LEFT]},
                {m_input.menuClickAction, menuClickPath[Side::RIGHT]},
                {m_input.vibrateAction, hapticPath[Side::LEFT]},
                {m_input.vibrateAction, hapticPath[Side::RIGHT]}
            }};

            const XrInteractionProfileSuggestedBinding suggestedBindings = 
            {
                .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                .next = NULL,
                .interactionProfile = oculusTouchInteractionProfilePath,
                .countSuggestedBindings = (uint32_t)bindings.size(),
                .suggestedBindings = bindings.data()
            };
            
            result = xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
            if (!xr_result(xrInstance, result, "Failed to suggest Oculus Touch bindings"))
                return;
        }

        // Suggest bindings for the Vive Controller.
        {
            XrPath viveControllerInteractionProfilePath;
            result = xrStringToPath(xrInstance, "/interaction_profiles/htc/vive_controller", &viveControllerInteractionProfilePath);
            if (!xr_result(xrInstance, result, "Failed to get Oculus Touch interaction profile"))
                return;
            std::vector<XrActionSuggestedBinding> bindings
            {{
                {m_input.grabAction, triggerValuePath[Side::LEFT]},
                {m_input.grabAction, triggerValuePath[Side::RIGHT]},
                {m_input.poseAction, posePath[Side::LEFT]},
                {m_input.poseAction, posePath[Side::RIGHT]},
                {m_input.thumbstickAction, posePath[Side::LEFT]},
                {m_input.thumbstickAction, posePath[Side::RIGHT]},
                {m_input.menuClickAction, menuClickPath[Side::LEFT]},
                {m_input.menuClickAction, menuClickPath[Side::RIGHT]},
                {m_input.vibrateAction, hapticPath[Side::LEFT]},
                {m_input.vibrateAction, hapticPath[Side::RIGHT]}
            }};
            XrInteractionProfileSuggestedBinding suggestedBindings
            {
                .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                .next = NULL,
                .interactionProfile = viveControllerInteractionProfilePath,
                .countSuggestedBindings = (uint32_t)bindings.size(),
                .suggestedBindings = bindings.data()
            };
            
            result = xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
            if (!xr_result(xrInstance, result, "Failed to suggest Vive bindings"))
                return;
        }
        
        // Suggest bindings for the Valve Index Controller.
        {
            XrPath valveIndexControllerInteractionProfilePath;
            result = xrStringToPath(xrInstance, "/interaction_profiles/valve/index_controller",
                                    &valveIndexControllerInteractionProfilePath);
            if (!xr_result(xrInstance, result, "Failed to get Valve Index interaction profile"))
                return;

            std::vector<XrActionSuggestedBinding> bindings
            {{
                {m_input.grabAction, triggerValuePath[Side::LEFT]},
                {m_input.grabAction, triggerValuePath[Side::RIGHT]},
                {m_input.poseAction, posePath[Side::LEFT]},
                {m_input.poseAction, posePath[Side::RIGHT]},
                {m_input.thumbstickAction, posePath[Side::LEFT]},
                {m_input.thumbstickAction, posePath[Side::RIGHT]},
                {m_input.menuClickAction, menuClickPath[Side::LEFT]},
                {m_input.menuClickAction, menuClickPath[Side::RIGHT]},
                {m_input.vibrateAction, hapticPath[Side::LEFT]},
                {m_input.vibrateAction, hapticPath[Side::RIGHT]}
            }};

            const XrInteractionProfileSuggestedBinding suggestedBindings = 
            {
                .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                .next = NULL,
                .interactionProfile = valveIndexControllerInteractionProfilePath,
                .countSuggestedBindings = (uint32_t)bindings.size(),
                .suggestedBindings = bindings.data()
            };

            result = xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
            if (!xr_result(xrInstance, result, "failed to suggest Valve Index bindings"))
                return;
        }

        // Suggest bindings for the Microsoft Mixed Reality Motion Controller.
        {
            XrPath microsoftMixedRealityInteractionProfilePath;
            result = xrStringToPath(xrInstance, "/interaction_profiles/microsoft/motion_controller",
                                       &microsoftMixedRealityInteractionProfilePath);
            if (!xr_result(xrInstance, result, "Failed to get Microsoft Mixed Reality interaction profile"))
                return;
            std::vector<XrActionSuggestedBinding> bindings
            {{
                {m_input.grabAction, squeezeClickPath[Side::LEFT]},
                {m_input.grabAction, squeezeClickPath[Side::RIGHT]},
                {m_input.poseAction, posePath[Side::LEFT]},
                {m_input.poseAction, posePath[Side::RIGHT]},
                {m_input.thumbstickAction, posePath[Side::LEFT]},
                {m_input.thumbstickAction, posePath[Side::RIGHT]},
                {m_input.menuClickAction, menuClickPath[Side::LEFT]},
                {m_input.menuClickAction, menuClickPath[Side::RIGHT]},
                {m_input.vibrateAction, hapticPath[Side::LEFT]},
                {m_input.vibrateAction, hapticPath[Side::RIGHT]}
            }};

            const XrInteractionProfileSuggestedBinding suggestedBindings = 
            {
                .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                .next = NULL,
                .interactionProfile = microsoftMixedRealityInteractionProfilePath,
                .countSuggestedBindings = (uint32_t)bindings.size(),
                .suggestedBindings = bindings.data()
            };

            result = xrSuggestInteractionProfileBindings(xrInstance, &suggestedBindings);
            if (!xr_result(xrInstance, result, "failed to suggest Microsoft Mixed Reality bindings"))
                return;
        }

        //[t] We cannot get the action poses until we create an action space for each. Then we can query.
        {
            XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            actionSpaceInfo.action = m_input.poseAction; 
            actionSpaceInfo.poseInActionSpace.orientation = {.x = 0, .y = 0, .z = 0, .w = 1.0};
            actionSpaceInfo.poseInActionSpace.position = {.x = 0, .y = 0, .z = 0};
            actionSpaceInfo.next = NULL;
            actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];

            result = xrCreateActionSpace(xrSession, &actionSpaceInfo, &m_input.handSpace[Side::LEFT]);
            if (!xr_result(xrInstance, result, "Failed to create left hand pose action space"))
                return;

            actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
            result = xrCreateActionSpace(xrSession, &actionSpaceInfo, &m_input.handSpace[Side::RIGHT]);
            if (!xr_result(xrInstance, result, "Failed to create right hand pose action space"))
                return;
        }

        XrSessionActionSetsAttachInfo xrSessionAattachInfo = {
            .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
            .next = NULL,
            .countActionSets = 1,
            .actionSets = &m_input.actionSet
            };
        result = xrAttachSessionActionSets(xrSession, &xrSessionAattachInfo);
        if (!xr_result(xrInstance, result, "Failed to attach Session Action Set"))
            return;
    }

    //[t] Poll all actions and poses.
    //[t] To be called before the xr wait for frame, or at the very least before xr BeginFrame.
    void QueryXRInputForAllDevices(XrInstance xrInstance, XrSession xrSession){
        XrResult result;
		// Sync actions
        const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};

		XrActionsSyncInfo actions_sync_info = {
			.type = XR_TYPE_ACTIONS_SYNC_INFO,
			.countActiveActionSets = 1,
			.activeActionSets = &activeActionSet,
		};
		result = xrSyncActions(xrSession, &actions_sync_info);
		xr_result(xrInstance, result, "Failed to Sync Actions");

    }
};

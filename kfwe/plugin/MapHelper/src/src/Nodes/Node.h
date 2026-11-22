#pragma once
#include "..\stdafx.h"
#include <memory>
#include <functional>


int __fastcall fakeGetChildCount(Action* action);
Action::Type get_action_type(Action* action);



namespace mh {

	typedef std::shared_ptr<class Node> NodePtr;

	class Node :public std::enable_shared_from_this<Node> {
	public:
		enum class TYPE :int {
			ROOT, //root node
			CALL, //No return value required
			GET,  //Return value required
			PARAM, //It is a parameterizer
			CLOSURE, //An incredible closure
		};

		//Get the current trigger
		virtual void* getData() = 0;

		//Get the current node name
		virtual const std::string& getName() = 0;

		//Current name id
		virtual uint32_t getNameId() = 0;

		//Node type
		virtual TYPE getType() = 0;

		virtual void setType(TYPE type) = 0;

		//Get parent node
		virtual NodePtr getParentNode() = 0;

		//Get the root node
		virtual NodePtr getRootNode() = 0;

		//Get the list of sub-actions
		virtual std::vector<NodePtr> getChildList() = 0;

		//Filter
		typedef std::function<bool(NodePtr node)> NodeFilter;
		//Get value
		virtual bool getValue(const NodeFilter& filter) = 0;

		//Get text
		virtual std::string toString(class TriggerFunction* func = nullptr) = 0;

		//Generate function name
		virtual std::string getFuncName() = 0;

		//Trigger variable name
		virtual const std::string& getTriggerVariableName() = 0;

		//Information about unbelievable local variables
		struct Upvalue{

			//Unbelievable local variable type
			enum class TYPE :int {
				SET_LOCAL,
				GET_LOCAL,
				SET_ARRAY,
				GET_ARRAY,
			};

			TYPE uptype;
			std::string name;
			std::string type;
			std::string value;
			std::string index;

			bool is_func = false;
		};

		//Generate incredible local variable code
		virtual std::string getUpvalue(const Upvalue& info) = 0;
	};


	NodePtr NodeFromTrigger(Trigger* trigger);

	NodePtr NodeFromAction(Action* action, uint32_t childId, NodePtr parent);

	NodePtr NodeFramParameter(Parameter* parameter, uint32_t index, NodePtr parent);




	//Triggers that need to be initialized
	extern std::unordered_map<Trigger*, bool> g_initTriggerMap;
	//Triggers that need to be blocked
	extern std::unordered_map<Trigger*, bool> g_disableTriggerMap;


	typedef NodePtr(*MakeNode)(void* action, uint32_t childId, NodePtr parent);

	extern std::unordered_map<std::string, MakeNode> MakeActionNodeMap;
	extern std::unordered_map<std::string, MakeNode> MakeParameterNodeMap;

}


#pragma once


namespace mh {


	typedef std::shared_ptr<class SingleNodeClosure> SingleNodeClosurePtr;
	//closure
	class SingleNodeClosure : public ClosureNode {
		REGISTER_FROM_CLOSUER(SingleNodeClosure)
	public:

		SingleNodeClosure(NodePtr node, const std::string& return_type, NodePtr parent)
			:SingleNodeClosure((Action*)node->getData(), 0, node)
		{
			m_node = node;
			m_return_type = return_type;
		}

		//There are no overpowered parameters, so -1
		virtual int getCrossDomainIndex() { return -1; }


		virtual std::vector<NodePtr> getChildList() {
			std::vector<NodePtr> list;

			NodePtr node = shared_from_this();

			//Add an action to the current closure
			NodePtr fake = NodeFromAction(m_action, 0, node);

			if (m_return_type != "nothing") {
				fake->setType(TYPE::GET);
			}
			list.push_back(fake);
			return list;
		}
		virtual std::string toString(TriggerFunction* func) override {

			auto& editor = get_trigger_editor();
			// script_name
			std::string name = editor.getScriptName(m_action);
			std::string func_name = getParentNode()->getFuncName();

			std::string result;

			//Compile the action into a function and return the function name.
			result =  func_name;

			std::string upvalues;
			std::vector<std::string> actions;

			FunctionPtr code = getBlock(func, func_name, m_return_type, upvalues, actions);

			func->addFunction(code);

			return result;
		}

		virtual std::string getUpvalue(const Upvalue& info) override {
			auto ptr = getParentNode();

			
			NodePtr last_closure = nullptr;
			getValue([&](NodePtr ptr) {
				if (ptr.get() != this && ptr->getType() == TYPE::CLOSURE) {
					auto node = std::dynamic_pointer_cast<ClosureNode>(ptr);
					if (node->params_finish) {
						last_closure = ptr;
						return true;
					}
				}
				return false;
			});

			if (last_closure) {
				return last_closure->getUpvalue(info);
			} 

			std::string result;

			switch (info.uptype) {
			case Upvalue::TYPE::SET_LOCAL:
				result = std::format("YDLocal2Set({}, \"{}\", {})", info.type, info.name, info.value);
				break;
			case Upvalue::TYPE::GET_LOCAL:
				result = std::format("YDLocal2Get({}, \"{}\")", info.type, info.name);
				break;
			case Upvalue::TYPE::SET_ARRAY:
				result = std::format("YDLocal2ArraySet({}, \"{}\", {}, {})", info.type, info.name, info.index, info.value);
				break;
			case Upvalue::TYPE::GET_ARRAY:
				result = std::format("YDLocal2ArrayGet({}, \"{}\", {})", info.type, info.name, info.index);
				break;
			default:
				break;
			}

			return result;
		}

	private:
		NodePtr m_node;
		std::string m_return_type;
	};


	class ForForceMultiple : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(ForForceMultiple)

		//There are no overpowered parameters, so -1
		virtual int getCrossDomainIndex() { return -1; }

		virtual std::string toString(TriggerFunction* func) override {

			auto& editor = get_trigger_editor();
			// script_name
			std::string name = editor.getScriptName(m_action);
			std::string func_name = getFuncName() + "A";

			std::string result = func->getSpaces() + "call " + name + "(";

			

			params_finish = false;

			for (auto& param : getParameterList()) {
				result += " " + param->toString(func);
				result += ",";
			}
			result += "function " + func_name + ")\n";

			params_finish = true;

			std::string upvalues;
			std::vector<std::string> upactions;
			FunctionPtr code = getBlock(func, func_name, "nothing", upvalues, upactions);

			for (auto& action : upactions) {
				result += action;
			}

			func->addFunction(code);
			return result;
		}

		virtual std::string getUpvalue(const Upvalue& info) override {

			if (!params_finish) { //If it's an action in the parameters, then let them access the parent level.
				return getParentNode()->getUpvalue(info);
			}

			NodePtr last_closure = nullptr;
			getValue([&](NodePtr ptr) {
				if (ptr.get() != this && ptr->getType() == TYPE::CLOSURE) {
					last_closure = ptr;
					return true;
				}
				return false;
			});
			if (last_closure) { //If there is a parent closure, the environment within that parent closure is used; otherwise, in the root trigger, the following rules apply.
				return last_closure->getUpvalue(info);
			} else if (info.is_func){
				//If it's a root trigger and you want to get a function value, then directly use the value from the root trigger.
				return getRootNode()->getUpvalue(info);
			}

			std::string result;
			

			switch (info.uptype) {
			case Upvalue::TYPE::SET_LOCAL:
				result = std::format("YDLocal2Set({}, \"{}\", {})", info.type, info.name, info.value);
				break;
			case Upvalue::TYPE::GET_LOCAL:
				result = std::format("YDLocal2Get({}, \"{}\")", info.type, info.name);
				break;
			case Upvalue::TYPE::SET_ARRAY:
				result = std::format("YDLocal2ArraySet({}, \"{}\", {}, {})", info.type, info.name, info.index, info.value);
				break;
			case Upvalue::TYPE::GET_ARRAY:
				result = std::format("YDLocal2ArrayGet({}, \"{}\", {})", info.type, info.name, info.index);
				break;
			default:
				break;
			}
			return result;
		}

	};


	class YDWEExecuteTriggerMultiple : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(YDWEExecuteTriggerMultiple)

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			//Add local variables
			func->current()->addLocal("ydl_triggerstep", "integer");
			func->current()->addLocal("ydl_trigger", "trigger", std::string(), false);

			std::string result;

			params_finish = false; 
			result += func->getSpaces() + "set ydl_trigger = " + params[0]->toString(func) + "\n";
			result += func->getSpaces() + "YDLocalExecuteTrigger(ydl_trigger)\n";

			params_finish = true;
			for (auto& node : getChildList()) {
				result += node->toString(func);
			}
			result += func->getSpaces() + "call YDTriggerExecuteTrigger(ydl_trigger, " + params[1]->toString(func) + ")\n";

			return result;
		}


		virtual std::string getUpvalue(const Upvalue& info) override {
			if (!params_finish) { //If it's an action in the parameters, then let them access the parent level.
				return getParentNode()->getUpvalue(info);
			}

			std::string result;

			switch (info.uptype) {
			case Upvalue::TYPE::SET_LOCAL:
				result = std::format("YDLocal5Set({}, \"{}\", {})", info.type, info.name, info.value);
				break;
			case Upvalue::TYPE::GET_LOCAL:
				result = getParentNode()->getUpvalue(info);
				break;
			case Upvalue::TYPE::SET_ARRAY:
				result = std::format("YDLocal5ArraySet({}, \"{}\", {}, {})", info.type, info.name, info.index, info.value);
				break;
			case Upvalue::TYPE::GET_ARRAY:
				result = getParentNode()->getUpvalue(info);
				break;
			default:
				break;
			}
			return result;
		}
	};

	//ydwe's unbelievable timer
	class YDWETimerStartMultiple : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(YDWETimerStartMultiple)

		virtual int getCrossDomainIndex() override { return 0; }

		//Does it automatically pass unrestricted local variables, similar to cross-domain references within closures?
		virtual bool isVariableCrossDomain() { return true; }

		//Whether to automatically pass, get the triggering unit, get the triggering player, these function values
		virtual bool isFunctionCrossDomain() { return true; }

		std::string getHandleName() {
			if (getCurrentGroupId() <= getCrossDomainIndex()) {
				return "ydl_timer";
			}
			return "GetExpiredTimer()";
		}

		virtual std::string getUpvalue(const Upvalue& info) override {
			if (!params_finish) { //If it's an action in the parameters, then let them access the parent level.
				return getParentNode()->getUpvalue(info);
			}

			std::string result;

			bool is_get = info.uptype == Upvalue::TYPE::GET_LOCAL || info.uptype == Upvalue::TYPE::GET_ARRAY;
			if (getCrossDomainIndex() == getCurrentGroupId() && is_get) {	//If the current area is a parameter passing section, then use the local variables from the previous level.
				result = getParentNode()->getUpvalue(info);
				return result;
			}

			switch (info.uptype) {
			case Upvalue::TYPE::SET_LOCAL:
				result = std::format("YDLocalSet({}, {}, \"{}\", {})", getHandleName(), info.type, info.name, info.value);
				break;
			case Upvalue::TYPE::GET_LOCAL:
				result = std::format("YDLocalGet({}, {}, \"{}\")", getHandleName(), info.type, info.name);
				break;
			case Upvalue::TYPE::SET_ARRAY:
				result = std::format("YDLocalArraySet({}, {}, \"{}\", {}, {})", getHandleName(), info.type, info.name, info.index, info.value);
				break;
			case Upvalue::TYPE::GET_ARRAY:
				result = std::format("YDLocalArrayGet({}, {}, \"{}\", {})", getHandleName(), info.type, info.name, info.index);
				break;
			default:
				break;
			}
			return result;
		}

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			//Add local variables
			func->current()->addLocal("ydl_timer", "timer", std::string(), false);

			std::string result;
			std::string save_state; 
			std::string start;

			std::string func_name = getFuncName() + "T";
			params_finish = false; 

			result = func->getSpaces() + "set ydl_timer = " + params[0]->toString(func) + "\n";
			start = func->getSpaces() + "call TimerStart(ydl_timer, "
				+ params[1]->toString(func) + ", "
				+ params[2]->toString(func)
				+ ", function " + func_name + ")\n";

			params_finish = true;

			std::vector<std::string> upactions(getCrossDomainIndex() + 1);

			FunctionPtr closure = getBlock(func, func_name, "nothing", save_state, upactions); // Создание функции
			result += upactions[0]; //Parameter area
			result += save_state;	//Automatic parameter passing
			result += start;		//Start timer

			func->addFunction(closure); // Финальное добавление функции

			return result;
		}


	};

	class PK_JassFuncCreateNN : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(PK_JassFuncCreateNN)

		//There are no overpowered parameters, so -1
		virtual int getCrossDomainIndex() { return -1; }

		virtual std::string toString(TriggerFunction* func) override {

			auto& editor = get_trigger_editor();
			// script_name
			std::string name = editor.getScriptName(m_action);
			auto params = getParameterList();
			std::string func_name = params[0]->toString(func);

			std::string result = "";

			

			params_finish = false;
			

			params_finish = true;

			std::string upvalues;
			std::vector<std::string> upactions;
			FunctionPtr code = getBlock(func, func_name, "nothing", upvalues, upactions);

			for (auto& action : upactions) {
				result += action;
			}

			func->addFunction(code);
			return result;
		}

	};

	class PK_JassFuncCreateAN : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(PK_JassFuncCreateAN)

		//There are no overpowered parameters, so -1
		virtual int getCrossDomainIndex() { return -1; }

		virtual std::string toString(TriggerFunction* func) override {

			auto& editor = get_trigger_editor();
			// script_name
			std::string name = editor.getScriptName(m_action);
			auto params = getParameterList();
			std::string func_name = params[0]->toString(func);

			std::string result = "";
			std::string custom = "takes " + params[1]->toString(func) + " returns nothing";
			



			std::string upvalues;
			std::vector<std::string> upactions;
			FunctionPtr code = getBlock(func, func_name, "custom " + custom, upvalues, upactions);

			for (auto& action : upactions) {
				result += action;
			}

			func->addFunction(code);
			return result;
		}

	};

	class PK_JassFuncCreateAA : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(PK_JassFuncCreateAA)

		//There are no overpowered parameters, so -1
		virtual int getCrossDomainIndex() { return -1; }

		virtual std::string toString(TriggerFunction* func) override {

			auto& editor = get_trigger_editor();
			// script_name
			std::string name = editor.getScriptName(m_action);
			auto params = getParameterList();
			//typename_01_integer  + 11 = integer

			std::string func_name = params[0]->toString(func);

			std::string result = "";
			std::string custom = "takes " + params[1]->toString(func) + " returns " + std::string(m_action->parameters[2]->value + 11);

			std::string upvalues;
			std::vector<std::string> upactions;
			FunctionPtr code = getBlock(func, func_name, "custom " + custom, upvalues, upactions);

			for (auto& action : upactions) {
				result += action;
			}

			func->addFunction(code);
			return result;
		}

	};

	class PK_JassFuncCreateNA : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(PK_JassFuncCreateNA)

		//There are no overpowered parameters, so -1
		virtual int getCrossDomainIndex() { return -1; }

		virtual std::string toString(TriggerFunction* func) override {

			auto& editor = get_trigger_editor();
			// script_name
			std::string name = editor.getScriptName(m_action);
			auto params = getParameterList();
			//typename_01_integer  + 11 = integer

			std::string func_name = params[0]->toString(func);

			std::string result = "";
			std::string custom = "takes nothing returns " + std::string(m_action->parameters[1]->value + 11);

			std::string upvalues;
			std::vector<std::string> upactions;
			FunctionPtr code = getBlock(func, func_name, "custom " + custom, upvalues, upactions);

			for (auto& action : upactions) {
				result += action;
			}

			func->addFunction(code);
			return result;
		}

	};


	//ydwe's unbelievable trigger
	class YDWERegisterTriggerMultiple : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(YDWERegisterTriggerMultiple)

		virtual int getCrossDomainIndex() override { return 1; }

		//Does it automatically pass unrestricted local variables, similar to cross-domain references within closures?
		virtual bool isVariableCrossDomain() { return true; }

		//Whether to automatically pass, get the triggering unit, get the triggering player, these function values
		virtual bool isFunctionCrossDomain() { return false; }

		std::string getHandleName() {
			if (getCurrentGroupId() <= getCrossDomainIndex()) {
				return "ydl_trigger";
			}
			return "GetTriggeringTrigger()";
		}

		virtual std::string getUpvalue(const Upvalue& info) override {
			std::string result;

			if (!params_finish) { //If it's an action in the parameters, then let them access the parent level.
				return getParentNode()->getUpvalue(info);
			}

			bool is_get = info.uptype == Upvalue::TYPE::GET_LOCAL || info.uptype == Upvalue::TYPE::GET_ARRAY;

			//If the current area is a parameter passing section, then use the local variables from the previous level.
			bool is_param_block = (getCrossDomainIndex() == getCurrentGroupId() && is_get);

			//The event also uses variables from the previous level.
			bool is_event_block = getCurrentGroupId() < getCrossDomainIndex();

			//If it's a function value and cross-domain function values ​​are not currently allowed, also use the parent domain.
			bool is_func = info.is_func && !isFunctionCrossDomain();

			if (is_param_block || is_event_block || is_func) {
				result = getParentNode()->getUpvalue(info);
				return result;
			}
		
			switch (info.uptype) {
			case Upvalue::TYPE::SET_LOCAL:
				result = std::format("YDLocalSet({}, {}, \"{}\", {})", getHandleName(), info.type, info.name, info.value);
				break;
			case Upvalue::TYPE::GET_LOCAL:
				result = std::format("YDLocalGet({}, {}, \"{}\")", getHandleName(), info.type, info.name);
				break;
			case Upvalue::TYPE::SET_ARRAY:
				result = std::format("YDLocalArraySet({}, {}, \"{}\", {}, {})", getHandleName(), info.type, info.name, info.index, info.value);
				break;
			case Upvalue::TYPE::GET_ARRAY:
				result = std::format("YDLocalArrayGet({}, {}, \"{}\", {})", getHandleName(), info.type, info.name, info.index);
				break;
			default:
				break;
			}
			return result;
		}

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			//Add local variables
			func->current()->addLocal("ydl_trigger", "trigger", std::string(), false);

			std::string result;
			std::string save_state;

			std::string func_name = getFuncName() + "Conditions";
	
			params_finish = false;

			result += func->getSpaces() + "set ydl_trigger = " + params[0]->toString(func) + "\n";

			params_finish = true;

			std::vector<std::string> upactions(getCrossDomainIndex() + 1);

			FunctionPtr closure = getBlock(func, func_name, "nothing", save_state, upactions);
			result += upactions[1]; //Parameter area
			result += save_state;	//Automatic parameter passing code
			result += upactions[0];	//Event Area
			result += func->getSpaces() + "call TriggerAddCondition( ydl_trigger, Condition(function " + func_name + "))\n";


			func->addFunction(closure);

			return result;
		}

	};


	//Asynchronous Hardware Events in NetEase Editor
	class DzTriggerRegisterMouseEventMultiple : public ClosureNode {
	public:
		REGISTER_FROM_CLOSUER(DzTriggerRegisterMouseEventMultiple)

		virtual int getCrossDomainIndex() override { return 0; }

		//Does it automatically pass unrestricted local variables, similar to cross-domain references within closures?
		virtual bool isVariableCrossDomain() { return true; }

		//Whether to automatically pass, get the triggering unit, get the triggering player, these function values
		virtual bool isFunctionCrossDomain() { return false; }

		virtual std::string getUpvalue(const Upvalue& info) override {
			std::string result;


			if (!params_finish) { //If it's an action in the parameters, then let them access the parent level.
				return getParentNode()->getUpvalue(info);
			}

			bool is_get = info.uptype == Upvalue::TYPE::GET_LOCAL || info.uptype == Upvalue::TYPE::GET_ARRAY;
			if ((getCrossDomainIndex() == getCurrentGroupId() && is_get)|| !params_finish) {	//If the current area is a parameter passing section, then use the local variables from the previous level.
				result = getParentNode()->getUpvalue(info);
				return result;
			}
	
			std::string func_name = getFuncName();

			switch (info.uptype) {
			case Upvalue::TYPE::SET_LOCAL:
				result = std::format("YDLocal6Set(\"{}\", {}, \"{}\", {})", func_name, info.type, info.name, info.value);
				break;
			case Upvalue::TYPE::GET_LOCAL:
				result = std::format("YDLocal6Get(\"{}\", {}, \"{}\")", func_name, info.type, info.name);
				break;
			case Upvalue::TYPE::SET_ARRAY:
				result = std::format("YDLocal6ArraySet(\"{}\", {}, \"{}\", {}, {})", func_name, info.type, info.name, info.index, info.value);
				break;
			case Upvalue::TYPE::GET_ARRAY:
				result = std::format("YDLocal6ArrayGet(\"{}\", {}, \"{}\", {})", func_name, info.type, info.name, info.index);
				break;
			default:
				break;
			}
			return result;
		}

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			std::string action_name = getName();
			action_name = action_name.replace(action_name.find("Multiple"), -1, "ByCode");

			std::string result;

			params_finish = false;

			std::string action;
			action += func->getSpaces() + "if GetLocalPlayer() == " + params[0]->toString(func) + " then\n";
			func->addSpace();
			action += getScript(func, action_name, params);
			func->subSpace();
			action += func->getSpaces() + "endif\n";


			std::string upvalues;

			params_finish = true;

			std::vector<std::string> upactions(getCrossDomainIndex() + 1);

			FunctionPtr closure = getBlock(func, m_function, "nothing", upvalues, upactions);
			func->addFunction(closure);
			result += upactions[0]; //Parameter area
			result += upvalues; //Automatic parameter passing code
			result += action;

			return result;
		}

		virtual std::string getScript(TriggerFunction* func, const std::string& name, const std::vector<NodePtr>& params) {

			std::string result;
			std::vector<std::string> args;

			std::string key = "T";
			switch (m_nameId) {
			case "DzTriggerRegisterMouseEventMultiple"s_hash:
				key = "MT";  break;
			case "DzTriggerRegisterKeyEventMultiple"s_hash:
				key = "KT";  break;
			case "DzTriggerRegisterMouseWheelEventMultiple"s_hash:
				key = "WT";  break;
			case "DzTriggerRegisterMouseMoveEventMultiple"s_hash:
				key = "MMT";  break;
			case "DzTriggerRegisterWindowResizeEventMultiple"s_hash:
				key = "WRT";  break;
			case "DzFrameSetUpdateCallbackMultiple"s_hash:
				key = "CT";  break;
				break;
			case "DzFrameSetScriptMultiple"s_hash:
				key = "FT";  break;
			default:
				break;
			}
			m_function = getFuncName() + key;

			switch (m_nameId) {
			case "DzTriggerRegisterMouseEventMultiple"s_hash:
			case "DzTriggerRegisterKeyEventMultiple"s_hash:
				args = { "null", params[2]->toString(func), params[1]->toString(func), "false", "function " + m_function };
				break;
			case "DzTriggerRegisterMouseWheelEventMultiple"s_hash:
			case "DzTriggerRegisterMouseMoveEventMultiple"s_hash:
			case "DzTriggerRegisterWindowResizeEventMultiple"s_hash:
				args = { "null", "false", "function " + m_function };
				break;
			case "DzFrameSetUpdateCallbackMultiple"s_hash:
				args = { "function " + m_function };
				break;
			case "DzFrameSetScriptMultiple"s_hash:
				args = { params[2]->toString(func), params[1]->toString(func), "function " + m_function, "false"};
				break;
			default:
				break;
			}
			result += func->getSpaces() + "call " + name + "( ";
			for (size_t i = 0; i < args.size(); i++) {
				auto& arg = args[i];
				result += arg;
				if (i < args.size() - 1) {
					result += ", ";
				}
			}
			result += ")\n";
			return  result;
		}

	private:
		std::string m_function;

	};
}
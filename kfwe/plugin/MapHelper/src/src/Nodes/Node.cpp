
#include "../stdafx.h"
#include <regex>
#include <memory>
#include <functional>
#include "../WorldEditor.h"
#include "../TriggerEditor.h"

#include "Function.hpp"
#include "Node.h"
#include "TriggerNode.hpp"
#include "ActionNode.hpp"
#include "ParameterNode.hpp"
#include "EventNode.hpp"
#include "ClosureNode.hpp"
#include "Closures.hpp"

BOOL g_bYDWEEnumUnitsInRangeMultipleFlag = FALSE;
namespace mh {
	typedef std::shared_ptr<class ActionNode> ActionNodePtr;

	std::unordered_map<Trigger*, bool> g_initTriggerMap;
	std::unordered_map<Trigger*, bool> g_disableTriggerMap;


	class BoolexprNode : public ParameterNode {
	public:
		REGISTER_FROM_PARAM(BoolexprNode)

		virtual std::string toString(TriggerFunction* func) override {
			NodePtr node = shared_from_this();

			NodePtr param = getParamActionNode();

			//Create a closure
			SingleNodeClosurePtr closure = SingleNodeClosurePtr(new SingleNodeClosure(param, "boolean", node));

			return  "Condition(function " + closure->toString(func) + ")";;
		}
	};

	class CodeNode : public ParameterNode {
	public:
		REGISTER_FROM_PARAM(CodeNode)

		virtual std::string toString(TriggerFunction* func) override {
			NodePtr node = shared_from_this();

			NodePtr param = getParamActionNode();
			param->setType(TYPE::CALL);
			//Create a closure
			SingleNodeClosurePtr closure = SingleNodeClosurePtr(new SingleNodeClosure(param, "nothing", node));


			return "function " + closure->toString(func);
		}

	};

	class CommentString : public ActionNode {
	public:
		REGISTER_FROM_ACTION(CommentString)

		virtual std::string toString(TriggerFunction* func) override {
			NodePtr node = shared_from_this();
			NodePtr param = NodeFramParameter(m_action->parameters[0], 0, node);

			return toLine("//" + param->toString(func), func);
		}
	};

	//class CustomScriptCode : public ActionNode {
	//public:
	//	REGISTER_FROM_ACTION(CustomScriptCode)
	//
	//	virtual std::string toString(TriggerFunction* func) override {
	//		return toLine(m_action->parameters[0]->value, func);
	//	}
	//};
	
	class GetTriggerName : public ActionNode {
	public:
		REGISTER_FROM_ACTION(GetTriggerName)

		virtual std::string toString(TriggerFunction* func) override {
			return "\"" + getRootNode()->getName() + "\"";
		}
	};
	
	class OperatorString : public ActionNode {
	public:
		REGISTER_FROM_ACTION(OperatorString)

		virtual std::string toString(TriggerFunction* func) override {
			auto list = getParameterList();
			return "(" + list[0]->toString(func) + " + " + list[1]->toString(func) + ")";
		}
	};
	
	
	class ForLoopA : public ActionNode {
	public:
		REGISTER_FROM_ACTION(ForLoopA)

		virtual std::string toString(TriggerFunction* func) override {
			std::string result;
	
			auto params = getParameterList();
	
			auto node = std::dynamic_pointer_cast<ParameterNode>(params[2])->getParamActionNode();
			node->setType(TYPE::CALL);

			std::string variable = m_nameId == "ForLoopA"s_hash ? "bj_forLoopAIndex" : "bj_forLoopBIndex";
		
			result += func->getSpaces() + "set " + variable + " = " + params[0]->toString(func) + "\n";
			result += func->getSpaces() + "loop\n";
			func->addSpace();
	
			result += func->getSpaces() + "exitwhen " + variable + ">" + params[1]->toString(func) + "\n";

			result += node->toString(func) + "\n";
			result += func->getSpaces() + "set " + variable + " = " + variable + " + 1\n";
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
	
			return result;
		}
	};
	
	
	class ForLoopVar : public ActionNode {
	public:
		REGISTER_FROM_ACTION(ForLoopVar)

		virtual std::string toString(TriggerFunction* func) override {
			std::string result;
	
			auto params = getParameterList();
	
			std::string variable = params[0]->toString(func);
	
			auto node = std::dynamic_pointer_cast<ParameterNode>(params[3])->getParamActionNode();
			node->setType(TYPE::CALL);

			result += func->getSpaces() + "set " + variable + " = " + params[1]->toString(func) + "\n";
			result += func->getSpaces() + "loop\n";
			func->addSpace();
	
			result += func->getSpaces() + "exitwhen " + variable + ">" + params[2]->toString(func) + "\n";
			result += node->toString(func) + "\n";
			result += func->getSpaces() + "set " + variable + " = " + variable + " + 1\n";
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			
	
			return result;
		}
	};
	
	class IfThenElse : public ActionNode {
	public:
		REGISTER_FROM_ACTION(IfThenElse)

		virtual std::string toString(TriggerFunction* func) override {
			std::string result;
			auto params = getParameterList();

			auto node0 = (std::dynamic_pointer_cast<ParameterNode>(params[0]))->getParamActionNode();
			auto node1 = (std::dynamic_pointer_cast<ParameterNode>(params[1]))->getParamActionNode();
			auto node2 = (std::dynamic_pointer_cast<ParameterNode>(params[2]))->getParamActionNode();

			node0->setType(TYPE::GET);
			node1->setType(TYPE::CALL);
			node2->setType(TYPE::CALL);

	
		
			result += func->getSpaces() + "if (" + node0->toString(func) + ") then\n";
			func->addSpace();
			result += node1->toString(func);
			func->subSpace();
			result += func->getSpaces() + "else\n";
			func->addSpace();
			result += node2->toString(func);
			func->subSpace();
			result += func->getSpaces() + "endif\n";
		;
		
			return result;
		}
	};

	class GetBooleanAnd : public ActionNode {
	public:
		REGISTER_FROM_ACTION(GetBooleanAnd)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return "(" + params[0]->toString(func) + " and " + params[1]->toString(func) + ")";
		}
	};

	class GetBooleanOr : public ActionNode {
	public:
		REGISTER_FROM_ACTION(GetBooleanOr)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return "(" + params[0]->toString(func) + " or " + params[1]->toString(func) + ")";
		}
	};

	class OperatorInt : public ActionNode {
	public:
		REGISTER_FROM_ACTION(OperatorInt)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return "(" + params[0]->toString(func) + " " + params[1]->toString(func) + " " + params[2]->toString(func) + ")";
		}
	};


	class OperatorCompare : public ActionNode {
	public:
		REGISTER_FROM_ACTION(OperatorCompare)

			virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return "(" + params[0]->toString(func) + " " + params[1]->toString(func) + " " + params[2]->toString(func) + ")";
		}
	};

	class WaitForCondition : public ActionNode {
	public:
		REGISTER_FROM_ACTION(WaitForCondition)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			auto node = std::dynamic_pointer_cast<ParameterNode>(params[0]);

			auto child = node->getParamActionNode();

			//����һ���հ�
			SingleNodeClosurePtr closure = SingleNodeClosurePtr(new SingleNodeClosure(child, "boolean", node));


			std::string result;
			result += func->getSpaces( ) + "loop\n";
			result += func->getSpaces(1) + "exitwhen (" + closure->toString(func) + "())\n";
			result += func->getSpaces(1) + "call TriggerSleepAction(RMaxBJ(bj_WAIT_FOR_COND_MIN_INTERVAL, " + params[1]->toString(func) + "))\n";
			result += func->getSpaces( ) + "endloop\n";

			return result;
		}
	};

	class ForLoopAMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(ForLoopAMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string loop_index = getName() == "ForLoopAMultiple" ? "bj_forLoopAIndex" : "bj_forLoopBIndex";
			std::string loop_index_end = getName() == "ForLoopAMultiple" ? "bj_forLoopAIndexEnd" : "bj_forLoopBIndexEnd";

			std::string result;
			result += func->getSpaces() + "set " + loop_index + " = " + params[0]->toString(func) + "\n";
			result += func->getSpaces() + "set " + loop_index_end + " = " + params[1]->toString(func) + "\n";
			result += func->getSpaces() + "loop\n";
			func->addSpace();
			result += func->getSpaces() + "exitwhen " + loop_index + " > " + loop_index_end + "\n";
			for (auto& child : getChildList()) {
				result += child->toString(func);
			}
			result += func->getSpaces() + "set " + loop_index + " = " + loop_index + " + 1\n";
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			return result;
		}
	};

	class ForLoopVarMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(ForLoopVarMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string variable = params[0]->toString(func);

			std::string result;
			result += func->getSpaces() + "set " + variable + " = " + params[1]->toString(func) + "\n";
			result += func->getSpaces() + "loop\n";
			func->addSpace();
			result += func->getSpaces() + "exitwhen " + variable + " > " + params[2]->toString(func) + "\n";
			for (auto& child : getChildList()) {
				result += child->toString(func);
			}
			result += func->getSpaces() + "set " + variable + " = " + variable + " + 1\n";
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			return result;
		}
	};

	class IfThenElseMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(IfThenElseMultiple)

		virtual std::string toString(TriggerFunction* func) override {

			std::string iftext, thentext, elsetext, result;
			std::vector<std::vector<NodePtr>> nodes(3);

			for (auto& node : getChildList()) {
				Action* action = (Action*)node->getData();
				uint32_t index = 0;
				if (get_action_type(action) == Action::Type::condition) {
					index = 0;
				} else {
					if (action->group_id == 1) {
						index = 1;
					}else {
						index = 2;
					}
				}
				nodes[index].push_back(node);
			}
			
			for (auto& node : nodes[0]) {
				if (!iftext.empty()) {
					iftext += " and ";
				}
				node->setType(TYPE::GET);
				iftext += node->toString(func);
			}
			func->addSpace();
			for (auto& node : nodes[2]) {
				elsetext += node->toString(func);
			}
			for (auto& node : nodes[1]) {
				thentext += node->toString(func);
			}
		
			func->subSpace();
			if (iftext.empty()) {
				iftext = "true";
			}
			result += func->getSpaces() + "if (" + iftext + ") then\n";
			result += thentext;
			result += func->getSpaces() + "else\n";
			result += elsetext;
			result += func->getSpaces() + "endif\n";

			return result;
		}
	};


	


	class AndMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(AndMultiple)

			virtual std::string toString(TriggerFunction* func) override {

			std::string op, iftext, default_text;

			if (m_nameId == "AndMultiple"s_hash) {
				op = " and ";
				default_text = "true";
			} else {
				op = " or ";
				default_text = "false";
			}
			for (auto& node : getChildList()) {
				if (!iftext.empty()) {
					iftext += op;
				}
				iftext +=  node->toString(func);
			}
			if (iftext.empty()) {
				iftext = default_text;
			}
			return "(" + iftext + ")";
		}
	};

	class SetVariable : public ActionNode {
	public:
		REGISTER_FROM_ACTION(SetVariable)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return func->getSpaces() + "set " + params[0]->toString(func) + " = " + params[1]->toString(func) + "\n";
		}
	};


	class AddTriggerEvent : public ActionNode {
	public:
		REGISTER_FROM_ACTION(AddTriggerEvent)

			virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string trg = params[0]->toString(func);


			ParameterNodePtr param1 = std::dynamic_pointer_cast<ParameterNode>(params[1]);
			ActionNodePtr node = std::dynamic_pointer_cast<ActionNode>(param1->getParamActionNode());
			Action* action = (Action*)(node->getData());

			auto& editor = get_trigger_editor();
			std::string name = editor.getScriptName(action);

			std::string result = func->getSpaces() + "call " + name + "(" + trg;
			for (auto& arg : node->getParameterList()) {
				result += ", " + arg->toString(func);
			}
			result += ")\n";

			return result;
		}
	};


	class YDWEForLoopLocVarMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEForLoopLocVarMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string variable = std::string("ydul_") + m_action->parameters[0]->value;
			convert_loop_var_name(variable);

			//Add a local variable
			func->current()->addLocal(variable, "integer", std::string(), false);
		
			std::string result;
			result += func->getSpaces() + "set " + variable + " = " + params[1]->toString(func) + "\n";
			result += func->getSpaces() + "loop\n";
			func->addSpace();
			result += func->getSpaces() + "exitwhen " + variable + " > " + params[2]->toString(func) + "\n";
			for (auto& node : getChildList()) {
				result += node->toString(func);
			}
			result += func->getSpaces() + "set " + variable + " = " + variable + " + 1\n";
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			return result;
		}
	};

	class YDWERegionMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWERegionMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			
			std::string result;

			for (auto& node : getChildList()) {
				result += node->toString(func);
			}
			return result;
		}
	};

	class YDWEEnumUnitsInRangeMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEEnumUnitsInRangeMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			std::string result;

			if (g_bYDWEEnumUnitsInRangeMultipleFlag) { //Nested use is not allowed
				result += func->getSpaces() + base::a2u("YDTrigger Error: Do not nested <Select All Units In AoE/Rect>\n");
				print("Warning: trigger[%s] The nested use of <Select All Units In AoE/Rect> is not allowed, please fix it as soon as possible.\n\n", base::u2a(getRootNode()->getName()).c_str());

				return result;
			}

			auto params = getParameterList();

			params_finish = false;

			//Add local variables
			//func->current()->addLocal("ydl_group", "group", std::string(), false);
			//func->current()->addLocal("ydl_unit", "unit", std::string(), false);
			/*result += func->getSpaces() + "set PK_ULGroup = PK_GRCreateGroup()\n";
			result += func->getSpaces() + "call GroupEnumUnitsInRange(PK_ULGroup, " \
				+ params[0]->toString(func) + ", " \
				+ params[1]->toString(func) + ", " \
				+ params[2]->toString(func) + ", null)\n";

			params_finish = true;
			in_block = true;
			result += func->getSpaces() + "loop\n";
			func->addSpace();
			result += func->getSpaces() + "set PK_ULUnit = FirstOfGroup(PK_ULGroup)\n";
			result += func->getSpaces() + "exitwhen PK_ULUnit == null\n";
			result += func->getSpaces() + "call GroupRemoveUnit(PK_ULGroup, PK_ULUnit)\n";

			for (auto& node : getChildList()) {
				result += node->toString(func);
			}
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			result += func->getSpaces() + "call PK_UnitLoopClear()\n";
			in_block = false;

			return result;*/

			func->current()->addLocal("ydl_group", "group", std::string(), false);
			func->current()->addLocal("ydl_unit", "unit", std::string(), false);
			result += func->getSpaces() + "set ydl_group = PK_GRCreateGroup()\n";
			result += func->getSpaces() + "call GroupEnumUnitsInRange(ydl_group, " \
				+ params[0]->toString(func) + ", " \
				+ params[1]->toString(func) + ", " \
				+ params[2]->toString(func) + ", null)\n";

			params_finish = true;
			in_block = true;
			g_bYDWEEnumUnitsInRangeMultipleFlag = true;
			result += func->getSpaces() + "loop\n";
			func->addSpace();
			result += func->getSpaces() + "set ydl_unit = FirstOfGroup(ydl_group)\n";
			result += func->getSpaces() + "exitwhen ydl_unit == null\n";
			result += func->getSpaces() + "call GroupRemoveUnit(ydl_group, ydl_unit)\n";

			for (auto& node : getChildList()) {
				result += node->toString(func);
			}
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			result += func->getSpaces() + "call PK_GRClearGroup(ydl_group)\n";
			in_block = false;
			g_bYDWEEnumUnitsInRangeMultipleFlag = false;
			return result;
		}

	public:
		bool params_finish = false;

		static bool in_block;
	};
	bool YDWEEnumUnitsInRangeMultiple::in_block = false;

	class YDWEEnumUnitsInRectMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEEnumUnitsInRectMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			std::string result;

			if (g_bYDWEEnumUnitsInRangeMultipleFlag) { //Nested use is not allowed
				result += func->getSpaces() + base::a2u("YDTrigger Error: Do not nested <Select All Units In AoE/Rect>\n");
				print("Warning: trigger[%s] The nested use of <Select All Units In AoE/Rect> is not allowed, please fix it as soon as possible.\n\n", base::u2a(getRootNode()->getName()).c_str());

				return result;
			}

			auto params = getParameterList();

			params_finish = false;

			//Add local variables
			//func->current()->addLocal("ydl_group", "group", std::string(), false);
			//func->current()->addLocal("ydl_unit", "unit", std::string(), false);
			/*result += func->getSpaces() + "set PK_ULGroup = PK_GRCreateGroup()\n";
			result += func->getSpaces() + "call GroupEnumUnitsInRect(PK_ULGroup, " \
				+ params[0]->toString(func) + ", null)\n";
			params_finish = true;
			in_block = true;
			result += func->getSpaces() + "loop\n";
			func->addSpace();
			result += func->getSpaces() + "set PK_ULUnit = FirstOfGroup(PK_ULGroup)\n";
			result += func->getSpaces() + "exitwhen PK_ULUnit == null\n";
			result += func->getSpaces() + "call GroupRemoveUnit(PK_ULGroup, PK_ULUnit)\n";

			for (auto& node : getChildList()) {
				result += node->toString(func);
			}
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			result += func->getSpaces() + "call PK_UnitLoopClear()\n";
			in_block = false;

			return result;*/

			func->current()->addLocal("ydl_group", "group", std::string(), false);
			func->current()->addLocal("ydl_unit", "unit", std::string(), false);
			result += func->getSpaces() + "set ydl_group = PK_GRCreateGroup()\n";
			result += func->getSpaces() + "call GroupEnumUnitsInRect(ydl_group, " \
				+ params[0]->toString(func) + ", null)\n";
			params_finish = true;
			in_block = true;
			g_bYDWEEnumUnitsInRangeMultipleFlag = true;
			result += func->getSpaces() + "loop\n";
			func->addSpace();
			result += func->getSpaces() + "set ydl_unit = FirstOfGroup(ydl_group)\n";
			result += func->getSpaces() + "exitwhen ydl_unit == null\n";
			result += func->getSpaces() + "call GroupRemoveUnit(ydl_group, ydl_unit)\n";

			for (auto& node : getChildList()) {
				result += node->toString(func);
			}
			func->subSpace();
			result += func->getSpaces() + "endloop\n";
			result += func->getSpaces() + "call PK_GRClearGroup(ydl_group)\n";
			in_block = false;
			g_bYDWEEnumUnitsInRangeMultipleFlag = false;
			return result;
		}

	public:
		bool params_finish = false;

		static bool in_block;
	};
	bool YDWEEnumUnitsInRectMultiple::in_block = false;

	//

	class PK_SetCreateUnitXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateUnitXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedUnit = CreateUnit(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateUnitLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateUnitLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedUnit = CreateUnitAtLoc(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateCorpseXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateCorpseXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedUnit = CreateCorpse(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateCorpseLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateCorpseLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedUnit = CreateCorpse(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", GetLocationX(" + params[2]->toString(func) + "), GetLocationY(" + params[2]->toString(func) + "), " + params[3]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetAddLightningXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetAddLightningXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedLightning = AddLightning(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ", " + params[5]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetAddLightningXYZ : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetAddLightningXYZ)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedLightning = AddLightningEx(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ", " + params[5]->toString(func) + ", " + params[6]->toString(func) + ", " + params[7]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetAddLightningLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetAddLightningLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedLightning = AddLightning(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", GetLocationX(" + params[2]->toString(func) + "), GetLocationY(" + params[2]->toString(func) + "), GetLocationX(" + params[3]->toString(func) + "), GetLocationY(" + params[3]->toString(func) + "))\n";
			return result;
		}
	};

	class PK_SetCreateDestructableXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateDestructableXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedDestructable = CreateDestructable(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ", " + params[5]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateDestructableLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateDestructableLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedDestructable = CreateDestructable(" + params[0]->toString(func) + ", GetLocationX(" + params[1]->toString(func) + "), GetLocationY(" + params[1]->toString(func) + "), " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateDeadDestructableXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateDeadDestructableXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedDestructable = CreateDeadDestructable(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ", " + params[5]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateDeadDestructableLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateDeadDestructableLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedDestructable = CreateDeadDestructable(" + params[0]->toString(func) + ", GetLocationX(" + params[1]->toString(func) + "), GetLocationY(" + params[1]->toString(func) + "), " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateItemXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateItemXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedItem = CreateItem(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateItemLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateItemLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedItem = CreateItem(" + params[0]->toString(func) + ", GetLocationX(" + params[1]->toString(func) + "), GetLocationY(" + params[1]->toString(func) + "))\n";
			return result;
		}
	};

	class PK_SetAddSpecialEffectXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetAddSpecialEffectXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedEffect = AddSpecialEffect(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetAddSpecialEffectLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetAddSpecialEffectLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedEffect = AddSpecialEffect(" + params[0]->toString(func) + ", GetLocationX(" + params[1]->toString(func) + "), GetLocationY(" + params[1]->toString(func) + "))\n";
			return result;
		}
	};

	class PK_SetAddSpecialEffectTarget : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetAddSpecialEffectTarget)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedEffect = AddSpecialEffectTarget(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetCreateUnitPool : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateUnitPool)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedUnitPool = CreateUnitPool()\n";
			return result;
		}
	};

	class PK_SetCreateItemPool : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetCreateItemPool)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastCreatedItemPool = CreateItemPool()\n";
			return result;
		}
	};

	class PK_SetPlaceRandomUnitXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetPlaceRandomUnitXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastRandomUnitFromPool = PlaceRandomUnit(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_SetPlaceRandomItemXY : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_SetPlaceRandomItemXY)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += func->getSpaces() + "set bj_lastRandomItemFromPool = PlaceRandomItem(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func) + ")\n";
			return result;
		}
	};

	class PK_CallCreateDestructableLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_CallCreateDestructableLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += "CreateDestructable(" + params[0]->toString(func) + ", GetLocationX(" + params[1]->toString(func) + "), GetLocationY(" + params[1]->toString(func) + "), " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ")";
			return result;
		}
	};

	class PK_CallCreateDeadDestructableLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_CallCreateDeadDestructableLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += "CreateDeadDestructable(" + params[0]->toString(func) + ", GetLocationX(" + params[1]->toString(func) + "), GetLocationY(" + params[1]->toString(func) + "), " + params[2]->toString(func) + ", " + params[3]->toString(func) + ", " + params[4]->toString(func) + ")";
			return result;
		}
	};

	class PK_CallCreateItemLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_CallCreateItemLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += "CreateItem(" + params[0]->toString(func) + ", GetLocationX(" + params[1]->toString(func) + "), GetLocationY(" + params[1]->toString(func) + "))";
			return result;
		}
	};

	class PK_CallAddLightningLoc : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_CallAddLightningLoc)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();
			std::string result;
			result += "AddLightning(" + params[0]->toString(func) + ", " + params[1]->toString(func) + ", GetLocationX(" + params[2]->toString(func) + "), GetLocationY(" + params[2]->toString(func) + "), GetLocationX(" + params[3]->toString(func) + "), GetLocationY(" + params[3]->toString(func) + "))";
			return result;
		}
	};
	//

	//
	class YDWESaveAnyTypeDataByUserData : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWESaveAnyTypeDataByUserData)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += "call YDUserDataSet(";
			result += std::string(m_action->parameters[0]->value + 11) + ", ";
			result += params[1]->toString(func) + ",";
			result += "\"" +std::string(m_action->parameters[2]->value) + "\", ";
			result += std::string(m_action->parameters[3]->value + 11) + ", ";
			result += params[4]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_JassIf : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassIf)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();
			std::string iftext;
			std::vector<std::vector<NodePtr>> nodes(1);
			for (auto& node : getChildList()) {
				Action* action = (Action*)node->getData();
				uint32_t index = 0;
				if (get_action_type(action) == Action::Type::condition) {
					index = 0;
				} else {
					if (action->group_id == 1) {
						index = 1;
					}else {
						index = 2;
					}
				}
				nodes[index].push_back(node);
			}
			
			for (auto& node : nodes[0]) {
				if (!iftext.empty()) {
					iftext += " and ";
				}
				node->setType(TYPE::GET);
				iftext += node->toString(func);
			}
			if (iftext.empty()) {
				iftext = "true";
			}
			result += func->getSpaces();
			result += params[0]->toString(func);
			result += " (" + iftext + ") then\n";
			return  result;
		}
	};

	class PK_JassElse : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassElse)

		virtual std::string toString(TriggerFunction* func) override {
			return  func->getSpaces() + "else\n";
		}
	};

	class PK_JassEndif : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassEndif)

		virtual std::string toString(TriggerFunction* func) override {
			return  func->getSpaces() + "endif\n";
		}
	};

	class PK_JassVarNoValue : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassVarNoValue)

		virtual std::string toString(TriggerFunction* func) override {
			return  "";
		}
	};

	class TimerStart : public ActionNode {
	public:
		REGISTER_FROM_ACTION(TimerStart)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call TimerStart(";
			result += params[0]->toString(func) + ", ";
			result += params[1]->toString(func) + ", ";
			result += params[2]->toString(func) + ", ";
			result += "function ";
			result += params[3]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_ForGroup : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_ForGroup)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call ForGroup(";
			result += params[0]->toString(func) + ", ";
			result += "function ";
			result += params[1]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_EnumUnitsInRangeMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_EnumUnitsInRangeMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();
			std::string bulka = (m_action->parameters[5]->value);
			std::string thentext;
			std::vector<std::vector<NodePtr>> nodes(1);

			for (auto& node : getChildList()) {
				Action* action = (Action*)node->getData();
				uint32_t index = 0;
				if (get_action_type(action) == Action::Type::action) {
					index = 0;
				} else {
					if (action->group_id == 1) {
						index = 1;
					}else {
						index = 2;
					}
				}
				nodes[index].push_back(node);
			}
			for (auto& node : nodes[0]) {
				thentext += node->toString(func);
			}
			//
			result += "set ";
			result += params[3]->toString(func) + " = ";
			if (bulka == "true")
			{
				result += "PK_GRCreateGroup()";
			}
			else
			{
				result += "CreateGroup()";
			}
			result += "\n" + func->getSpaces();
			result += "call GroupEnumUnitsInRange(";
			result += params[3]->toString(func) + ", ";
			result += params[0]->toString(func) + ", ";
			result += params[1]->toString(func) + ", ";
			result += params[2]->toString(func) + ", null)\n";
			result += func->getSpaces() + "loop\n";
			result += func->getSpaces() + "exitwhen loopUnit == null\n";
			result += func->getSpaces() + "set loopUnit = FirstOfGroup(";
			result += params[3]->toString(func) + ")\n";
			result += func->getSpaces() + "call GroupRemoveUnit(";
			result += params[3]->toString(func) + ", ";
			result += params[4]->toString(func) + ")\n";
			result += thentext;
			result += func->getSpaces() + "endloop\n" + func->getSpaces();
			if (bulka == "true")
			{
				result += "call PK_GRClearGroup(";
			}
			else
			{
				result += "call DestroyGroup(";
			}
			result += params[3]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_EnumUnitsInRectMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_EnumUnitsInRectMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();
			std::string bulka = (m_action->parameters[3]->value);
			std::string thentext;
			std::vector<std::vector<NodePtr>> nodes(1);

			for (auto& node : getChildList()) {
				Action* action = (Action*)node->getData();
				uint32_t index = 0;
				if (get_action_type(action) == Action::Type::action) {
					index = 0;
				} else {
					if (action->group_id == 1) {
						index = 1;
					}else {
						index = 2;
					}
				}
				nodes[index].push_back(node);
			}
			for (auto& node : nodes[0]) {
				thentext += node->toString(func);
			}
			//
			result += "set ";
			result += params[1]->toString(func) + " = ";
			if (bulka == "true")
			{
				result += "PK_GRCreateGroup()";
			}
			else
			{
				result += "CreateGroup()";
			}
			result += "\n" + func->getSpaces();
			result += "call GroupEnumUnitsInRect(";
			result += params[1]->toString(func) + ", ";
			result += params[0]->toString(func) + ", null)\n";
			result += func->getSpaces() + "loop\n";
			result += func->getSpaces() + "exitwhen loopUnit == null\n";
			result += func->getSpaces() + "set loopUnit = FirstOfGroup(";
			result += params[1]->toString(func) + ")\n";
			result += func->getSpaces() + "call GroupRemoveUnit(";
			result += params[1]->toString(func) + ", ";
			result += params[2]->toString(func) + ")\n";
			result += thentext;
			result += func->getSpaces() + "endloop\n" + func->getSpaces();
			if (bulka == "true")
			{
				result += "call PK_GRClearGroup(";
			}
			else
			{
				result += "call DestroyGroup(";
			}
			result += params[1]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_GroupEnumUnitsInRange : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_GroupEnumUnitsInRange)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call GroupEnumUnitsInRange(";
			result += params[0]->toString(func) + ", ";
			result += params[1]->toString(func) + ", ";
			result += params[2]->toString(func) + ", ";
			result += params[3]->toString(func) + ", function ";
			result += params[4]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_GroupEnumUnitsInRect : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_GroupEnumUnitsInRect)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call GroupEnumUnitsInRect(";
			result += params[0]->toString(func) + ", ";
			result += params[1]->toString(func) + ", function ";
			result += params[2]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_TriggerAddCondition : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_TriggerAddCondition)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call TriggerAddCondition(";
			result += params[0]->toString(func) + ", function ";
			result += params[1]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_EnumItemsInRect : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_EnumItemsInRect)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call EnumItemsInRect(";
			result += params[0]->toString(func) + ", null, function ";
			result += params[1]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_EnumDestructablesInRectAllMultiple : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_EnumDestructablesInRectAllMultiple)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call EnumDestructablesInRect(";
			result += params[0]->toString(func) + ", null, function ";
			result += params[1]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_ForForce : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_ForForce)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "call ForForce(";
			result += params[0]->toString(func) + ", function ";
			result += params[1]->toString(func) + ")\n";
			return  result;
		}
	};

	class PK_LocalLoopCondition : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_LocalLoopCondition)

		virtual std::string toString(TriggerFunction* func) override {

			std::string result = func->getSpaces();
			std::string iftext, thentext;
			std::vector<std::vector<NodePtr>> nodes(2);

			for (auto& node : getChildList()) {
				Action* action = (Action*)node->getData();
				uint32_t index = 0;
				if (get_action_type(action) == Action::Type::condition) {
					index = 0;
				} else {
					if (action->group_id == 1) {
						index = 1;
					}else {
						index = 2;
					}
				}
				nodes[index].push_back(node);
			}
			
			for (auto& node : nodes[0]) {
				if (!iftext.empty()) {
					iftext += " and ";
				}
				node->setType(TYPE::GET);
				iftext += node->toString(func);
			}
			func->addSpace();
			for (auto& node : nodes[1]) {
				thentext += node->toString(func);
			}
		
			func->subSpace();
			if (iftext.empty()) {
				iftext = "true";
			}

			result += "loop\n";
			result += func->getSpaces() + "exitwhen (" + iftext + ")\n";
			result += thentext;
			result += func->getSpaces() + "endloop\n";
			return  result;
		}
	};

	class PK_JassReturnN : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassReturnN)

		virtual std::string toString(TriggerFunction* func) override {
			return  func->getSpaces() + "return\n";
		}
	};

	class PK_JassGlobalInitBlock : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassGlobalInitBlock)

		virtual std::string toString(TriggerFunction* func) override {

			std::string result = func->getSpaces();
			std::string thentext;
			std::vector<std::vector<NodePtr>> nodes(1);

			for (auto& node : getChildList()) {
				Action* action = (Action*)node->getData();
				uint32_t index = 0;
				if (get_action_type(action) == Action::Type::action) {
					index = 0;
				} else {
					if (action->group_id == 1) {
						index = 1;
					}else {
						index = 2;
					}
				}
				nodes[index].push_back(node);
			}
			
			func->addSpace();
			for (auto& node : nodes[0]) {
				thentext += node->toString(func);
			}
		
			result += "globals\n";
			result += thentext + "\n";
			result += "endglobals\n";
			return  result;
		}
	};

	class PK_JassLocalSet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassLocalSet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();
			std::string strValue = (m_action->parameters[0]->value);

			if (strValue == "pk_jasslocal0") // Init
			{
				result += "local ";
				result += std::string(m_action->parameters[1]->value + 11) + " ";
				result += params[2]->toString(func);
				if (std::string(m_action->parameters[3]->value) != "PK_JassVarNoValue") // No value
				{
					result += " = " + params[3]->toString(func);
				}
			}
			else // Set
			{
				result += "set ";
				result += params[2]->toString(func);
				result += " = " + params[3]->toString(func);
			}
			result += "\n";
			return  result;
		}
	};

	class PK_JassLocalArrayInit : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassLocalArrayInit)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += "local ";
			result += std::string(m_action->parameters[0]->value + 11) + " array ";
			result += params[1]->toString(func) + "\n";
			return  result;
		}
	};

	class PK_JassLocalArraySet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassLocalArraySet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += "set ";
			result += params[1]->toString(func) + "[";
			result += params[2]->toString(func) + "] = ";
			result += params[3]->toString(func) + "\n";
			return  result;
		}
	};

	class PK_JassGlobalSet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassGlobalSet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();
			std::string strValue = (m_action->parameters[0]->value);

			if (strValue == "pk_jasslocal0") // Init
			{
				result += std::string(m_action->parameters[1]->value + 11) + " ";
				result += params[2]->toString(func);
				if (std::string(m_action->parameters[3]->value) != "PK_JassVarNoValue") // No value
				{
					result += " = " + params[3]->toString(func);
				}
			}
			else // Set
			{
				result += "set ";
				result += params[2]->toString(func);
				result += " = " + params[3]->toString(func);
			}
			result += "\n";
			return  result;
		}
	};

	class PK_JassGlobalArrayInit : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassGlobalArrayInit)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += std::string(m_action->parameters[0]->value + 11) + " array ";
			result += params[1]->toString(func) + "\n";
			return  result;
		}
	};

	class PK_JassGlobalArraySet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassGlobalArraySet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += "set ";
			result += params[1]->toString(func) + "[";
			result += params[2]->toString(func) + "] = ";
			result += params[3]->toString(func) + "\n";
			return  result;
		}
	};

	class PK_JassGlobalArray2DInit : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassGlobalArray2DInit)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += std::string(m_action->parameters[0]->value + 11) + " array ";
			result += params[1]->toString(func) + "[";
			result += params[2]->toString(func) + "][";
			result += params[3]->toString(func) + "]";
			return  result;
		}
	};

	class PK_JassGlobalArray2DSet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassGlobalArray2DSet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "set ";
			result += params[1]->toString(func) + "[";
			result += params[2]->toString(func) + "][";
			result += params[3]->toString(func) + "] = ";
			result += params[4]->toString(func) + "\n";
			return  result;
		}
	};

	class PK_JassReturnValue : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassReturnValue)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result = func->getSpaces();

			result += "return ";
			result += params[1]->toString(func) + "\n";
			return  result;
		}
	};

	class PK_JassVarGet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassVarGet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[0]->toString(func);
		}
	};

	class PK_JassVarArrayGet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassVarArrayGet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[0]->toString(func) + "[" + params[1]->toString(func) + "]";
		}
	};

	class PK_JassVarArray2DGet : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassVarArray2DGet)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[0]->toString(func) + "[" + params[1]->toString(func) + "][" + params[2]->toString(func) + "]";
		}
	};

	class PK_JassFuncRunNN : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassFuncRunNN)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  func->getSpaces() + "call " + params[0]->toString(func) + "()\n";
		}
	};

	class PK_JassFuncRunAN : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassFuncRunAN)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  func->getSpaces() + "call " + params[0]->toString(func) + "(" + params[1]->toString(func) + ")\n";
		}
	};

	class PK_JassFuncRunNA : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassFuncRunNA)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[0]->toString(func) + "()";
		}
	};

	class PK_JassFuncRunAA : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassFuncRunAA)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[0]->toString(func) + "(" + params[1]->toString(func) + ")";
		}
	};

	class PK_JassFuncArg2Run : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassFuncArg2Run)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[1]->toString(func);
		}
	};

	class PK_JassFuncArg2RunLv2 : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassFuncArg2RunLv2)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[0]->toString(func) + ", " + params[1]->toString(func);
		}
	};

	class PK_JassFuncArg2RunLv3 : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_JassFuncArg2RunLv3)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			return  params[0]->toString(func) + ", " + params[1]->toString(func) + ", " + params[2]->toString(func);
		}
	};

	class PK_IgnoreMapHelper : public ActionNode {
	public:
		REGISTER_FROM_ACTION(PK_IgnoreMapHelper)

		virtual std::string toString(TriggerFunction* func) override {
			return  "";
		}
	};


	class YDWEFlushAnyTypeDataByUserData : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEFlushAnyTypeDataByUserData)

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += "call YDUserDataClear(";
			result += std::string(m_action->parameters[0]->value + 11) + ", ";
			result += params[1]->toString(func) + ",";
			result += "\"" + std::string(m_action->parameters[3]->value) + "\", ";
			result += std::string(m_action->parameters[2]->value + 11) + ")\n";
	
			return result;
		}
	};

	class YDWEFlushAllByUserData : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEFlushAllByUserData)

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			//typename_01_integer  + 11 = integer
			std::string result = func->getSpaces();

			result += "call YDUserDataClearTable(" 
				+ std::string(m_action->parameters[0]->value + 11) 
				+ ", " 
				+ params[1]->toString(func) 
				+ ")\n";
			return result;
		}
	};

	


	class YDWETimerStartFlush : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWETimerStartFlush)

		virtual std::string toString(TriggerFunction* func) override {
			bool in_block = false;

			getValue([&](NodePtr ptr) {
				if (ptr->getType() == TYPE::CLOSURE) {
					auto node = std::dynamic_pointer_cast<ClosureNode>(ptr);
					if (node->getCrossDomainIndex() != -1) {
						if (ptr->getNameId() == "YDWETimerStartMultiple"s_hash) {
							in_block = true;
						}
						return true;
					}
				}
				return false;
			});

			std::string result;
			if (in_block) {
				result += func->getSpaces() + "call YDLocal3Release()\n";
				result += func->getSpaces() + "call PauseTimer(GetExpiredTimer())\n";
				result += func->getSpaces() + "call DestroyTimer(GetExpiredTimer())\n";
			} else {
				result += func->getSpaces() + base::a2u("YDTrigger Error: Do not use <Local Timer - Clear> outside of the actions of Local Timer.\n");
				print("Warning: trigger[%s] Using <Local Timer - Clear> outside of the Timer's action. Please fix this as soon as possible.\n\n", base::u2a(getRootNode()->getName()).c_str());
			}
			
			return result;
		}
	};

	class YDWERegisterTriggerFlush : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWERegisterTriggerFlush)

		virtual std::string toString(TriggerFunction* func) override {
			bool in_block = false;

			getValue([&](NodePtr ptr) {
				if (ptr->getType() == TYPE::CLOSURE) {
					auto node = std::dynamic_pointer_cast<ClosureNode>(ptr);
					if (node->getCrossDomainIndex() != -1) {
						if (ptr->getNameId() == "YDWERegisterTriggerMultiple"s_hash) {
							in_block = true;
						}
						return true;
					}
				}
				return false;
			});

			std::string result;
			if (in_block) {
				result += func->getSpaces() + "call YDLocal4Release()\n";
				result += func->getSpaces() + "call DestroyTrigger(GetTriggeringTrigger())\n";
			} else {
				result += func->getSpaces() + base::a2u("YDTrigger Error: Don’t use <Local Trigger - Clear> outside of the actions of the Local Trigger.\n");
				print("Warning: trigger[%s] Using <Local Trigger - Clear> outside of the Trigger's action. Please fix this as soon as possible.\n\n", base::u2a(getRootNode()->getName()).c_str());
			}
			
			return result;
		}
	};

	class TriggerSleepAction : public ActionNode {
	public:
		REGISTER_FROM_ACTION(TriggerSleepAction)

			virtual std::string toString(TriggerFunction* func) override {
			bool in_block = false;

			NodePtr parent = nullptr;

			getValue([&](NodePtr ptr) {
				if ((ptr->getType() == TYPE::CLOSURE || ptr->getType() == TYPE::ROOT)) {
					if (ptr->getNameId() == "YDWETimerStartMultiple"s_hash || ptr->getNameId() == "YDWERegisterTriggerMultiple"s_hash) {
						in_block = true;
					}
					parent = ptr;
				}
				return false;
			});

			std::string result;
			if (in_block) {
				result += func->getSpaces() + base::a2u("YDTrigger Error: Do not use wait in <Unbelievable> local timer/trigger.\n");
				print("Warning: trigger[%s] Using Wait in <Unbelievable> local timer/trigger actions. Please fix this as soon as possible.", base::u2a(getRootNode()->getName()).c_str());
			} else {
				result = ActionNode::toString(func);
			}

			if (parent == nullptr || parent->getType() == TYPE::ROOT) {
				auto node = std::dynamic_pointer_cast<TriggerNode>(getRootNode());
				node->hasUpvalue = true;
				result += "call YDLocalReset()\n";
			}
			return result;
		}
	};

	class ReturnAction : public ActionNode {
	public:
		REGISTER_FROM_ACTION(ReturnAction)

		virtual std::string toString(TriggerFunction* func) override {

			std::string result = func->getSpaces();
			if (g_bYDWEEnumUnitsInRangeMultipleFlag) { 
				result += base::a2u("call PK_GRClearGroup(ydl_group)\n");
			}

			result += "__RETURN__" 
				+ func->getSpaces() + "return\n";

			return result;
		}
	};


	class YDWEExitLoop : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEExitLoop)

		virtual std::string toString(TriggerFunction* func) override {
			std::string result = func->getSpaces();
			// По факту не нужно т.к это не ретюрн и после этого функция не обрывается
			//if (g_bYDWEEnumUnitsInRangeMultipleFlag) { 
			//	result += base::a2u("call PK_GRClearGroup(ydl_group)\n");
			//}
			result += "exitwhen true\n";
			return result;
		}
	};

	class DoNothing : public ActionNode {
	public:
		REGISTER_FROM_ACTION(DoNothing)

		virtual std::string toString(TriggerFunction* func) override {
			return "";
		}
	};

	class CustomScriptCode : public ActionNode {
	public:
		REGISTER_FROM_ACTION(CustomScriptCode)

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();
			std::string script = params[0]->toString(func);

			if (getType() == TYPE::CALL) { //Split the local variable declarations in the custom value.
				
				//Matching local variable arrays
				std::regex reg("^\\s*local\\s+(\\w+)\\s+array\\s+(\\w+)");
		
				auto it_end = std::sregex_iterator();
				auto it = std::sregex_iterator(script.begin(), script.end(), reg);
				for (; it != it_end; ++it) {
					func->current()->addLocal(it->str(2), it->str(1), std::string(), true);
				}

				//match local variables
				reg = std::regex("^\\s*local\\s+(\\w+)\\s+(\\w+)");
				it = std::sregex_iterator(script.begin(), script.end(), reg);
				for (; it != it_end; ++it) {
					if (it->str(2) != "array") {
						func->current()->addLocal(it->str(2), it->str(1), std::string(), false);
					}
				}

				//Delete the array declaration.
				script = regex_replace(script, std::regex("^\\s*local\\s+\\w+\\s+array\\s+\\w+\\s*"), "");

				//Deleting the declaration part of the local variable declaration assignment
				script = regex_replace(script, std::regex("^\\s*local\\s+\\w+\\s+(\\w+)\\s*="), "set $1 =");

				//Delete the local variable declarations.
				script = regex_replace(script, std::regex("^\\s*local\\s+\\w+\\s+\\w+\\s*"), "");	
			}


			auto root = std::dynamic_pointer_cast<TriggerNode>(getRootNode());
			return toLine(script, func);
		}
	};


	class YDWEActivateTrigger : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEActivateTrigger)

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			Parameter* param = m_action->parameters[0];

			std::string result;

			if (param->typeId == Parameter::Type::variable && param->arrayParam == NULL) {
				const char* ptr = param->value;
				if (ptr && strncmp(ptr, "gg_trg_", 7) == 0)
					ptr = ptr + 7;
				std::string func_name = std::string("InitTrig_") + ptr;
				convert_name(func_name);

				std::string ret = params[1]->toString(func);
				if (ret.compare("true") == 0) {
					result += "call ExecuteFunc(\"" + func_name + "\")\n";
				} else {
					result += "call " + func_name + "()\n";
				}
			}
			return result;
		}
	};




	class YDWELoadAnyTypeDataByUserData : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWELoadAnyTypeDataByUserData)

		virtual std::string toString(TriggerFunction* func) override {

			auto params = getParameterList();

			ParameterNodePtr parent = std::dynamic_pointer_cast<ParameterNode>(getParentNode());
			Parameter* parent_parameter = (Parameter*)parent->getData();

			std::string result;
			result += "YDUserDataGet(";
			result += m_action->parameters[0]->value + 11;//typename_01_integer  + 11 = integer
			result += ", ";
			result += params[1]->toString(func);
			result += ",\"";
			result += m_action->parameters[2]->value;
			result += "\", ";
			result += parent_parameter->type_name;
			result += ")";
			return result;
		}
	};


	class YDWEHaveSavedAnyTypeDataByUserData : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEHaveSavedAnyTypeDataByUserData)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			std::string result;
			result += "YDUserDataHas(";
			result += m_action->parameters[0]->value + 11;//typename_01_integer  + 11 = integer
			result += ", ";
			result += params[1]->toString(func);
			result += ",\"";
			result += m_action->parameters[3]->value;
			result += "\", ";
			result += m_action->parameters[2]->value + 11;
			result += ")";
			return result;
		}
	};

	class GetEnumUnit : public ActionNode {
	public:
		REGISTER_FROM_ACTION(GetEnumUnit)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			bool in_block = false;
			bool in_closure = false;

			getValue([&](NodePtr ptr) {
				if (ptr->getNameId() == "YDWEEnumUnitsInRangeMultiple"s_hash) {
					auto node = std::dynamic_pointer_cast<YDWEEnumUnitsInRangeMultiple>(ptr);
					if (node->params_finish) {
						in_block = true;
					}
					return true;
				}
				if (ptr->getNameId() == "YDWEEnumUnitsInRectMultiple"s_hash) {
					auto node = std::dynamic_pointer_cast<YDWEEnumUnitsInRectMultiple>(ptr);
					if (node->params_finish) {
						in_block = true;
					}
					return true;
				}

				if (ptr->getType() == TYPE::CLOSURE) {
					auto node = std::dynamic_pointer_cast<ClosureNode>(ptr);
					if (node->params_finish && node->getCrossDomainIndex() != -1 && node->getCurrentGroupId() > node->getCrossDomainIndex() ) {
						in_closure = true;
						return true;
					} else if (node->getCrossDomainIndex() == -1) {
						in_closure = true;
						return true;
					}
				}
				
				return false;
			});

			std::string result;

			if (!in_closure && in_block) {
				result += "ydl_unit";
			} else {
				result += m_name + "()";
			}

			return result;
		}
	};


	class YDWEForLoopLocVarIndex : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEForLoopLocVarIndex)

	virtual std::string toString(TriggerFunction* func) override {
			std::string result = std::string("ydul_") + m_action->parameters[0]->value;
			convert_loop_var_name(result);

			return result;
		}
	};


	class YDWEGetAnyTypeLocalVariable : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEGetAnyTypeLocalVariable)

		virtual std::string toString(TriggerFunction* func) override {
			ParameterNodePtr parent = std::dynamic_pointer_cast<ParameterNode>(getParentNode());
			Parameter* parent_parameter = (Parameter*)parent->getData();

			Upvalue upvalue = { Upvalue::TYPE::GET_LOCAL };
			upvalue.name = m_action->parameters[0]->value;;
			upvalue.type = parent_parameter->type_name;;

			auto root = std::dynamic_pointer_cast<TriggerNode>(getRootNode());
			root->hasUpvalue = true;

			//Upload parameters
			getValue([&](NodePtr ptr) {
				if (ptr->getType() == TYPE::CLOSURE) {
					auto node = std::dynamic_pointer_cast<ClosureNode>(ptr);
					//If it's an action area and there's no parameter area, the default parameter passing is used.
					if (node->getCurrentGroupId() > node->getCrossDomainIndex() || node->getCrossDomainIndex() == -1) {
						node->upvalue_map.emplace(upvalue.name, upvalue);
						return true;
					} 

				} 
				return false;
			});

			
			std::string result = getUpvalue(upvalue);

			//Records unconventional variables for type checking.
			auto& map = root->all_upvalue_map[upvalue.name];
			auto& editor = get_trigger_editor();
			auto base = editor.getBaseType(upvalue.type);

			if (map.find(base) == map.end()) {
				if (!g_make_editor_data) {
					Action* action = (Action*)getData();
					NodePtr ptr = shared_from_this();
					while (ptr->getType() != TYPE::CALL && ptr->getType() != TYPE::CLOSURE) {
						ptr = ptr->getParentNode();
						if (!ptr || ptr->getType() == TYPE::ROOT) {
							break;
						}
						action = (Action*)ptr->getData();
					}
					map.emplace(base, TriggerNode::WarningInfo{ std::string(), action });
				} else {
					map.emplace(base, TriggerNode::WarningInfo{ result });
				}
			}

			return result;
		}
	};

	
	class YDWESetAnyTypeLocalVariable : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWESetAnyTypeLocalVariable)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			Upvalue upvalue = { Upvalue::TYPE::SET_LOCAL };
			upvalue.name = m_action->parameters[1]->value;;
			upvalue.type = m_action->parameters[0]->value + 11;;
			upvalue.value = params[2]->toString(func);;

			auto root = std::dynamic_pointer_cast<TriggerNode>(getRootNode());
			root->hasUpvalue = true;


			std::string result = "call " + getUpvalue(upvalue);

			//Take the initiative to declare
			getValue([&](NodePtr ptr) {
				if (ptr->getType() == TYPE::CLOSURE) {
					auto node = std::dynamic_pointer_cast<ClosureNode>(ptr);
					if (node->getCurrentGroupId() == node->getCrossDomainIndex()) {
						node->define_upvalue_map.emplace(upvalue.name, upvalue);
					}
					return true;
				}
				return false;
			});


			//Records unconventional variables for type checking.
			auto& map = root->all_upvalue_map[upvalue.name];
			auto& editor = get_trigger_editor();
			auto base = editor.getBaseType(upvalue.type);
			if (map.find(base) == map.end()) {
				if (!g_make_editor_data) {
					map.emplace(base, TriggerNode::WarningInfo{std::string(), (Action*)getData()});
				} else {
					map.emplace(base, TriggerNode::WarningInfo{ result });
				}
			}

			return func->getSpaces() + result + "\n";
		}
	};


	class YDWEGetAnyTypeLocalArray : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWEGetAnyTypeLocalArray)

		virtual std::string toString(TriggerFunction* func) override {

			ParameterNodePtr parent = std::dynamic_pointer_cast<ParameterNode>(getParentNode());
			Parameter* parent_parameter = (Parameter*)parent->getData();

			auto params = getParameterList();

			Upvalue upvalue = { Upvalue::TYPE::GET_ARRAY };
			upvalue.name = m_action->parameters[0]->value;;
			upvalue.type = parent_parameter->type_name;;
			upvalue.index = params[1]->toString(func);;

			auto root = std::dynamic_pointer_cast<TriggerNode>(getRootNode());
			root->hasUpvalue = true;


			std::string result = getUpvalue(upvalue);



			//Records unconventional variables for type checking.
			auto& map = root->all_upvalue_map[upvalue.name];
			auto& editor = get_trigger_editor();
			auto base = editor.getBaseType(upvalue.type);
			if (map.find(base) == map.end()) {
				if (!g_make_editor_data) {
					Action* action = (Action*)getData();
					NodePtr ptr = shared_from_this();
					while (ptr->getType() != TYPE::CALL && ptr->getType() != TYPE::CLOSURE) {
						ptr = ptr->getParentNode();
						if (!ptr || ptr->getType() == TYPE::ROOT) {
							break;
						}
						action = (Action*)ptr->getData();
					}
					map.emplace(base, TriggerNode::WarningInfo{ std::string(), action });
				} else {
					map.emplace(base, TriggerNode::WarningInfo{ result });
				}
			}

			return result;
		}
	};

	class YDWESetAnyTypeLocalArray : public ActionNode {
	public:
		REGISTER_FROM_ACTION(YDWESetAnyTypeLocalArray)

		virtual std::string toString(TriggerFunction* func) override {
			auto params = getParameterList();

			Upvalue upvalue = { Upvalue::TYPE::SET_ARRAY };
			upvalue.name = m_action->parameters[1]->value;
			upvalue.type = m_action->parameters[0]->value + 11;
			upvalue.index = params[2]->toString(func);
			upvalue.value = params[3]->toString(func);;

			auto root = std::dynamic_pointer_cast<TriggerNode>(getRootNode());
			root->hasUpvalue = true;

			std::string result = "call " + getUpvalue(upvalue);


			//Records unconventional variables for type checking.
			auto& map = root->all_upvalue_map[upvalue.name];
			auto& editor = get_trigger_editor();
			auto base = editor.getBaseType(upvalue.type);
			if (map.find(base) == map.end()) {
				if (!g_make_editor_data) {
					map.emplace(base, TriggerNode::WarningInfo{ std::string(), (Action*)getData() });
				} else {
					map.emplace(base, TriggerNode::WarningInfo{ result });
				}
			}

			return func->getSpaces() + result + "\n";
		}
	};


	//Cross-domain access allows the trigger API to access objects within timers.
	class CrossDomain : public ActionNode {
	public:
		REGISTER_FROM_ACTION(CrossDomain)

		virtual std::string toString(TriggerFunction* func) override {
			
			auto& editor = get_trigger_editor();

			ParameterNodePtr parent = std::dynamic_pointer_cast<ParameterNode>(getParentNode());
			Parameter* parent_parameter = (Parameter*)parent->getData();

			std::string result;

			Upvalue upvalue = { Upvalue::TYPE::GET_LOCAL };
			upvalue.name = m_name;
			upvalue.type = editor.getBaseType(parent_parameter->type_name);
			upvalue.is_func = true;

			getValue([&](NodePtr ptr) {
				auto type = ptr->getType();
				if (type == TYPE::CLOSURE) {
					auto node = std::dynamic_pointer_cast<ClosureNode>(ptr);
					if (node->isFunctionCrossDomain()) {
						//Upward delivery
						node->upvalue_map.emplace(upvalue.name, upvalue);
						if (node->getCurrentGroupId() > node->getCrossDomainIndex()) {
							result += node->getUpvalue(upvalue);
							return true;
						}
					} else {
						return true;
					}
				}
				return false;
			});


			//If the function value cannot be obtained from the parent scope, then it is called directly.
			if (result.empty()) {
				result = upvalue.name + "()";
			}
			return result;
		}
	};


	//Parameter type node
	std::unordered_map<std::string, MakeNode> MakeParameterNodeMap = {
		{"boolexpr",				BoolexprNode::From},
		{"code",					CodeNode::From},
	};

	//action node
	std::unordered_map<std::string, MakeNode> MakeActionNodeMap = {
		{"CommentString",			CommentString::From},
		//{"CustomScriptCode",		CustomScriptCode::From},
		{"GetTriggerName",			GetTriggerName::From},
		{"OperatorString",			OperatorString::From},

		{"ForLoopA",				ForLoopA::From},
		{"ForLoopB",				ForLoopA::From}, //Same as A

		{"ForLoopVar",				ForLoopVar::From},
		{"IfThenElse",				IfThenElse::From},
		{"GetBooleanAnd",			GetBooleanAnd::From},
		{"GetBooleanOr",			GetBooleanOr::From},

		{"OperatorInt",				OperatorInt::From},
		{"OperatorReal",			OperatorInt::From}, //Same as int
		{"OperatorDegree",			OperatorInt::From},
		
		{"OperatorCompare",			OperatorCompare::From},
		{"OperatorCompare",			OperatorCompare::From},
		{"WaitForCondition",		WaitForCondition::From},

		{"ForLoopAMultiple",		ForLoopAMultiple::From},
		{"ForLoopBMultiple",		ForLoopAMultiple::From}, //Same as a

		{"ForLoopVarMultiple",		ForLoopVarMultiple::From},
		{"IfThenElseMultiple",		IfThenElseMultiple::From},

		{"ForForceMultiple",					ForForceMultiple::From}, //These implementations are the same
		{"ForGroupMultiple",					ForForceMultiple::From},
		{"EnumDestructablesInRectAllMultiple",	ForForceMultiple::From},
		{"EnumDestructablesInCircleBJMultiple",	ForForceMultiple::From},
		{"EnumItemsInRectBJMultiple",			ForForceMultiple::From},

		{"AndMultiple",							AndMultiple::From},
		{"OrMultiple",							AndMultiple::From},
		{"SetVariable",							SetVariable::From},
		{"AddTriggerEvent",						AddTriggerEvent::From},


		{"YDWEForLoopLocVarMultiple",			YDWEForLoopLocVarMultiple::From },
		{"YDWERegionMultiple",					YDWERegionMultiple::From },
		{"YDWEEnumUnitsInRangeMultiple",		YDWEEnumUnitsInRangeMultiple::From },
		{"YDWEEnumUnitsInRectMultiple",		    YDWEEnumUnitsInRectMultiple::From },
		{"YDWESaveAnyTypeDataByUserData",		YDWESaveAnyTypeDataByUserData::From },
		{"YDWEFlushAnyTypeDataByUserData",		YDWEFlushAnyTypeDataByUserData::From },
		{"YDWEFlushAllByUserData",				YDWEFlushAllByUserData::From },
		{"YDWEExecuteTriggerMultiple",			YDWEExecuteTriggerMultiple::From },
		{"YDWETimerStartMultiple",				YDWETimerStartMultiple::From },
		{"YDWERegisterTriggerMultiple",			YDWERegisterTriggerMultiple::From },
		{"YDWETimerStartFlush",					YDWETimerStartFlush::From },

		{"YDWERegisterTriggerFlush",			YDWERegisterTriggerFlush::From },
		{"TriggerSleepAction",					TriggerSleepAction::From },
		{"PolledWait",							TriggerSleepAction::From },

		{"ReturnAction",						ReturnAction::From },
		{"YDWEExitLoop",						YDWEExitLoop::From },
		{"DoNothing",						    DoNothing::From },
		// PK
		{"PK_SetCreateUnitXY",				    PK_SetCreateUnitXY::From },
		{"PK_SetCreateUnitLoc",				    PK_SetCreateUnitLoc::From },
		{"PK_SetCreateCorpseXY",			    PK_SetCreateCorpseXY::From },
		{"PK_SetCreateCorpseLoc",				PK_SetCreateCorpseLoc::From },
		{"PK_SetAddLightningXY",				PK_SetAddLightningXY::From },
		{"PK_SetAddLightningXYZ",				PK_SetAddLightningXYZ::From },
		{"PK_SetAddLightningLoc",				PK_SetAddLightningLoc::From },
		{"PK_SetCreateDestructableXY",			PK_SetCreateDestructableXY::From },
		{"PK_SetCreateDestructableLoc",			PK_SetCreateDestructableLoc::From },
		{"PK_SetCreateDeadDestructableXY",		PK_SetCreateDeadDestructableXY::From },
		{"PK_SetCreateDeadDestructableLoc",		PK_SetCreateDeadDestructableLoc::From },
		{"PK_SetCreateItemXY",				    PK_SetCreateItemXY::From },
		{"PK_SetCreateItemLoc",				    PK_SetCreateItemLoc::From },
		{"PK_SetAddSpecialEffectXY",			PK_SetAddSpecialEffectXY::From },
		{"PK_SetAddSpecialEffectLoc",			PK_SetAddSpecialEffectLoc::From },
		{"PK_SetAddSpecialEffectTarget",		PK_SetAddSpecialEffectTarget::From },
		{"PK_SetCreateUnitPool",				PK_SetCreateUnitPool::From },
		{"PK_SetCreateItemPool",				PK_SetCreateItemPool::From },
		{"PK_SetPlaceRandomUnitXY",				PK_SetPlaceRandomUnitXY::From },
		{"PK_SetPlaceRandomItemXY",				PK_SetPlaceRandomItemXY::From },
		{"PK_CallCreateDestructableLoc",		PK_CallCreateDestructableLoc::From },
		{"PK_CallCreateDeadDestructableLoc",	PK_CallCreateDeadDestructableLoc::From },
		{"PK_CallCreateItemLoc",				PK_CallCreateItemLoc::From },
		{"PK_CallAddLightningLoc",				PK_CallAddLightningLoc::From },
		{"PK_JassIf",				            PK_JassIf::From },
		{"PK_JassElse",				            PK_JassElse::From },
		{"PK_JassEndif",				        PK_JassEndif::From },
	    {"PK_JassVarNoValue",				    PK_JassVarNoValue::From },
		{"TimerStart",				            TimerStart::From },
		{"PK_ForGroup",				            PK_ForGroup::From },
		{"PK_EnumUnitsInRangeMultiple",			PK_EnumUnitsInRangeMultiple::From },
		{"PK_EnumUnitsInRectMultiple",			PK_EnumUnitsInRectMultiple::From },
		{"PK_GroupEnumUnitsInRange",			PK_GroupEnumUnitsInRange::From },
		{"PK_GroupEnumUnitsInRect",				PK_GroupEnumUnitsInRect::From },
		{"PK_TriggerAddCondition",				PK_TriggerAddCondition::From },
		{"PK_EnumItemsInRect",				    PK_EnumItemsInRect::From },
		{"PK_EnumDestructablesInRectAllMultiple", PK_EnumDestructablesInRectAllMultiple::From },
		{"PK_ForForce",				            PK_ForForce::From },
		{"PK_LocalLoopCondition",				PK_LocalLoopCondition::From },
		{"PK_JassReturnN",				        PK_JassReturnN::From },
		{"PK_JassGlobalInitBlock",				PK_JassGlobalInitBlock::From },
		{"PK_JassLocalSet",				        PK_JassLocalSet::From },
		{"PK_JassLocalArrayInit",				PK_JassLocalArrayInit::From },
		{"PK_JassLocalArraySet",				PK_JassLocalArraySet::From },
		{"PK_JassGlobalSet",				    PK_JassGlobalSet::From },
		{"PK_JassGlobalArrayInit",				PK_JassGlobalArrayInit::From },
		{"PK_JassGlobalArraySet",				PK_JassGlobalArraySet::From },
		{"PK_JassGlobalArray2DInit",			PK_JassGlobalArray2DInit::From },
		{"PK_JassGlobalArray2DSet",				PK_JassGlobalArray2DSet::From },
		{"PK_JassReturnValue",				    PK_JassReturnValue::From },
		{"PK_JassVarGet",				        PK_JassVarGet::From },
		{"PK_JassVarArrayGet",				    PK_JassVarArrayGet::From },
		{"PK_JassVarArray2DGet",				PK_JassVarArray2DGet::From },
		{"PK_JassFuncRunNN",				    PK_JassFuncRunNN::From },
		{"PK_JassFuncRunAN",				    PK_JassFuncRunAN::From },
		{"PK_JassFuncRunNA",				    PK_JassFuncRunNA::From },
		{"PK_JassFuncRunAA",				    PK_JassFuncRunAA::From },
		{"PK_JassFuncArg2Run",				    PK_JassFuncArg2Run::From },
		{"PK_JassFuncArg2RunLv2",				PK_JassFuncArg2RunLv2::From },
		{"PK_JassFuncArg2RunLv3",				PK_JassFuncArg2RunLv3::From },
		{"PK_JassFuncCreateNN",				    PK_JassFuncCreateNN::From },
		{"PK_JassFuncCreateAN",				    PK_JassFuncCreateAN::From },
		{"PK_JassFuncCreateAA",				    PK_JassFuncCreateAA::From },
		{"PK_JassFuncCreateNA",				    PK_JassFuncCreateNA::From },
		{"PK_IgnoreMapHelper",				    PK_IgnoreMapHelper::From },
		

		{"CustomScriptCode",					CustomScriptCode::From },
		{"YDWECustomScriptCode",				CustomScriptCode::From },
		{"YDWEActivateTrigger",					YDWEActivateTrigger::From },


		{"DzTriggerRegisterMouseEventMultiple",			DzTriggerRegisterMouseEventMultiple::From },
		{"DzTriggerRegisterKeyEventMultiple",			DzTriggerRegisterMouseEventMultiple::From },
		{"DzTriggerRegisterMouseWheelEventMultiple",	DzTriggerRegisterMouseEventMultiple::From },
		{"DzTriggerRegisterMouseMoveEventMultiple",		DzTriggerRegisterMouseEventMultiple::From },
		{"DzTriggerRegisterWindowResizeEventMultiple",	DzTriggerRegisterMouseEventMultiple::From },
		{"DzFrameSetUpdateCallbackMultiple",			DzTriggerRegisterMouseEventMultiple::From },
		{"DzFrameSetScriptMultiple",					DzTriggerRegisterMouseEventMultiple::From },


		{"YDWELoadAnyTypeDataByUserData",			YDWELoadAnyTypeDataByUserData::From },
		{"YDWEHaveSavedAnyTypeDataByUserData",		YDWEHaveSavedAnyTypeDataByUserData::From },
		{"GetEnumUnit",								GetEnumUnit::From },
		{"GetFilterUnit",							GetEnumUnit::From },
		{"YDWEForLoopLocVarIndex",					YDWEForLoopLocVarIndex::From },

		{"YDWEGetAnyTypeLocalVariable",				YDWEGetAnyTypeLocalVariable::From},
		{"YDWESetAnyTypeLocalVariable",				YDWESetAnyTypeLocalVariable::From},
		{"YDWEGetAnyTypeLocalArray",				YDWEGetAnyTypeLocalArray::From},
		{"YDWESetAnyTypeLocalArray",				YDWESetAnyTypeLocalArray::From},

	

		{"GetTriggeringTrigger",					CrossDomain::From},
		{"GetTriggerEventId",						CrossDomain::From},
		{"GetEventGameState",						CrossDomain::From},
		{"GetWinningPlayer",						CrossDomain::From},
		{"GetTriggeringRegion",						CrossDomain::From},
		{"GetEnteringUnit",							CrossDomain::From},
		{"GetLeavingUnit",							CrossDomain::From},
		{"GetTriggeringTrackable",					CrossDomain::From},
		{"GetClickedButton",						CrossDomain::From},
		{"GetClickedDialog",						CrossDomain::From},
		{"GetTournamentFinishSoonTimeRemaining",	CrossDomain::From},
		{"GetTournamentFinishNowRule",				CrossDomain::From},
		{"GetTournamentFinishNowPlayer",			CrossDomain::From},
		{"GetSaveBasicFilename",					CrossDomain::From},
		{"GetTriggerPlayer",						CrossDomain::From},
		{"GetLevelingUnit",							CrossDomain::From},
		{"GetLearningUnit",							CrossDomain::From},
		{"GetLearnedSkill",							CrossDomain::From},
		{"GetLearnedSkillLevel",					CrossDomain::From},
		{"GetRevivableUnit",						CrossDomain::From},
		{"GetRevivingUnit",							CrossDomain::From},
		{"GetAttacker",								CrossDomain::From},
		{"GetRescuer",								CrossDomain::From},
		{"GetDyingUnit",							CrossDomain::From},
		{"GetKillingUnit",							CrossDomain::From},
		{"GetDecayingUnit",							CrossDomain::From},
		{"GetConstructingStructure",				CrossDomain::From},
		{"GetCancelledStructure",					CrossDomain::From},
		{"GetConstructedStructure",					CrossDomain::From},
		{"GetResearchingUnit",						CrossDomain::From},
		{"GetResearched",							CrossDomain::From},
		{"GetTrainedUnitType",						CrossDomain::From},
		{"GetTrainedUnit",							CrossDomain::From},
		{"GetDetectedUnit",							CrossDomain::From},
		{"GetSummoningUnit",						CrossDomain::From},
		{"GetSummonedUnit",							CrossDomain::From},
		{"GetTransportUnit",						CrossDomain::From},
		{"GetLoadedUnit",							CrossDomain::From},
		{"GetSellingUnit",							CrossDomain::From},
		{"GetSoldUnit",								CrossDomain::From},
		{"GetBuyingUnit",							CrossDomain::From},
		{"GetSoldItem",								CrossDomain::From},
		{"GetChangingUnit",							CrossDomain::From},
		{"GetChangingUnitPrevOwner",				CrossDomain::From},
		{"GetManipulatingUnit",						CrossDomain::From},
		{"GetManipulatedItem",						CrossDomain::From},
		{"GetOrderedUnit",							CrossDomain::From},
		{"GetIssuedOrderId",						CrossDomain::From},
		{"GetOrderPointX",							CrossDomain::From},
		{"GetOrderPointY",							CrossDomain::From},
		{"GetOrderPointLoc",						CrossDomain::From},
		{"GetOrderTarget",							CrossDomain::From},
		{"GetOrderTargetDestructable",				CrossDomain::From},
		{"GetOrderTargetItem",						CrossDomain::From},
		{"GetOrderTargetUnit",						CrossDomain::From},
		{"GetSpellAbilityUnit",						CrossDomain::From},
		{"GetSpellAbilityId",						CrossDomain::From},
		{"GetSpellAbility",							CrossDomain::From},
		{"GetSpellTargetLoc",						CrossDomain::From},
		{"GetSpellTargetDestructable",				CrossDomain::From},
		{"GetSpellTargetItem",						CrossDomain::From},
		{"GetSpellTargetUnit",						CrossDomain::From},
		{"GetEventPlayerState",						CrossDomain::From},
		{"GetEventPlayerChatString",				CrossDomain::From},
		{"GetEventPlayerChatStringMatched",			CrossDomain::From},
		{"GetTriggerUnit",							CrossDomain::From},
		{"GetEventUnitState",						CrossDomain::From},
		{"GetEventDamage",							CrossDomain::From},
		{"GetEventDamageSource",					CrossDomain::From},
		{"GetEventDetectingPlayer",					CrossDomain::From},
		{"GetEventTargetUnit",						CrossDomain::From},
		{"GetTriggerWidget",						CrossDomain::From},
		{"GetTriggerDestructable",					CrossDomain::From},
	};

	



	NodePtr NodeFromTrigger(Trigger* trigger) {
		return TriggerNode::From(trigger, -1, nullptr);
	}

	NodePtr NodeFromAction(Action* action, uint32_t index, NodePtr parent) {

		MakeNode From = nullptr;

		std::string name = action->name;

		if (get_action_type(action) == Action::Type::event) {
			From = EventNode::From;
		} else if (MakeActionNodeMap.find(name) != MakeActionNodeMap.end()) {
			From = MakeActionNodeMap.at(name);
		} else if (name.substr(0, 15) == "OperatorCompare") {
			From = OperatorCompare::From;
		} else {
			From = ActionNode::From;
		}

		return From(action, index, parent);
	}

	NodePtr NodeFramParameter(Parameter* parameter, uint32_t index, NodePtr parent) {
		const auto child_type = std::string(parameter->type_name);

		auto it = MakeParameterNodeMap.find(child_type); //Preset types are available
		if (it != MakeParameterNodeMap.end()) {
			return it->second(parameter, index, parent);
		}
		return ParameterNode::From(parameter, index, parent);
	}


	bool IsHandleType(const std::string& type) {
		switch (hash_(type.c_str())) {
		case "nothing"s_hash:
		case "integer"s_hash:
		case "real"s_hash:
		case "boolean"s_hash:
		case "string"s_hash:
		case "code"s_hash:
			return false;
		default:
			break;
		}
		return true;
	}
}





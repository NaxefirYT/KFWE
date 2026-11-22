#pragma once
#include "Node.h"

namespace mh {


	//Generate a static method
#define REGISTER_FROM(name, type) static NodePtr From(void* param, uint32_t index, NodePtr parent) { return NodePtr(new name((type)param, index, parent)); } 

//Generate a static method + an ActionNode-based constructor.
#define REGISTER_FROM_ACTION(name) REGISTER_FROM(name, Action*); name(Action* param, uint32_t index, NodePtr parent): ActionNode(param, index, parent) { } ;


	class ActionNode : public Node {
	public:
		REGISTER_FROM(ActionNode, Action*)

		ActionNode(Action* action, uint32_t index, NodePtr parent) {
			m_action = action;

			m_parent = parent;

			m_index = index;

			m_name = std::string(action->name);
			m_nameId = hash_(action->name);

			if (parent->getType() == TYPE::PARAM || parent->getType() == TYPE::GET) {
				m_type = TYPE::GET;
			}
			else {
				m_type = TYPE::CALL;
			}

		
			if (parent->getType() == TYPE::ROOT) {
				m_root = parent;
			}
			else {
				m_root = parent->getRootNode();
			}
		}

		std::vector<NodePtr> getParameterList() {
			std::vector<NodePtr> list;

			NodePtr node = shared_from_this();

			uint32_t size = m_action->param_count;
			if (size > 0 && m_action->parameters) {
				for (uint32_t i = 0; i < size; i++) {
					Parameter* parameter = m_action->parameters[i];
					NodePtr param = NodeFramParameter(parameter, i, node);
					list.push_back(param);
				}
			}
			return list;
		}

		std::string toLine(const std::string& line, TriggerFunction* func) {
			if (getType() == TYPE::CALL) {
				return func->getSpaces() + line + "\n";
			}
			return line;
		}

		virtual void* getData() override { return m_action; }

		virtual const std::string& getName() override { return m_name; }

		virtual uint32_t getNameId() override { return m_nameId; }

		virtual TYPE getType() override { return m_type; }

		virtual void setType(TYPE type) override { m_type = type; }

		virtual NodePtr getParentNode() override { return m_parent; }

		virtual NodePtr getRootNode() override { return m_root; }

		virtual std::vector<NodePtr> getChildList() override {
			std::vector<NodePtr> list;

			if (m_action->child_count > 0 && m_action->child_actions) {

				NodePtr node = shared_from_this();

				for (uint32_t i = 0; i < m_action->child_count; i++)
				{
					Action* action = m_action->child_actions[i];

					if (!action->enable || !((int)action->group_id < fakeGetChildCount(m_action)))
						continue;
					NodePtr child = NodeFromAction(action,i, node);

					list.push_back(child);
				}
			}

			return list;
		}

		

		virtual bool getValue(const NodeFilter& filter) override {
			NodePtr node = shared_from_this();
			if (filter(node)) {
				return true;
			}
			else if (m_parent->getType() != TYPE::ROOT) { //pass upward
				return m_parent->getValue(filter);
			}
			return true;
		}

		virtual std::string toString(TriggerFunction* func) override {
			std::string result;

			if (getType() == TYPE::CALL) {
				result = "call ";
			}

			auto& editor = get_trigger_editor();
			std::string name = editor.getScriptName(m_action);

			bool res = getValue([&](NodePtr prev_node) {
				result += name;
				result += "(";
				auto list = getParameterList();
				uint32_t size = list.size();
				for (uint32_t i = 0; i < size; i++) {
					auto& param = list[i];
					result += " " + param->toString(func);
					if (i < size - 1) {
						result += ",";
					}
				}
				result += ")";
				return true;
			});
			
			return toLine(result, func);
		}


		
		virtual std::string getFuncName() override {
			auto ParentPtr = getParentNode();
			if (ParentPtr && ParentPtr->getType() == TYPE::PARAM) { //If it's an action within a parameter node, then the parameter's rules apply.
				return getParentNode()->getFuncName();
			} else { //If it is an action
				return std::format("{}Func{:03d}", getParentNode()->getFuncName(), m_index + 1);
			}
			return "";
		}

		virtual const std::string& getTriggerVariableName() override {
			return getParentNode()->getTriggerVariableName();
		}


		//Generate incredible local variable code
		virtual std::string getUpvalue(const Upvalue& info) override {
	
			return getParentNode()->getUpvalue(info);
		}

	protected:
		Action* m_action;
		TYPE m_type;

		NodePtr m_root;
		NodePtr m_parent;

		std::string m_name;
		uint32_t m_nameId;

		uint32_t m_index; //The ID of itself as a child action

	};
}
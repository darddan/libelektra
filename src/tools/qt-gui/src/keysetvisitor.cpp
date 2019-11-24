/**
 * @file
 *
 * @brief
 *
 * @copyright BSD License (see LICENSE.md or https://www.libelektra.org)
 */

#include "keysetvisitor.hpp"
#include "treeviewmodel.hpp"
#include "treemodel.hpp"

using namespace kdb;

KeySetVisitor::KeySetVisitor ()
{
}

void KeySetVisitor::visit (ConfigNode & node)
{
	Key key = node.getKey ();

	if (key && key.isValid ())
	{
		m_set.append (key);
	}
}

void KeySetVisitor::visit (TreeViewModel * model)
{
		foreach (ConfigNodePtr node, model->model ())
		{
			node->accept (*this);
		}
}

void KeySetVisitor::visit(TreeItem& item) {
	Key key = item.key ();

	if (key && key.isValid ()) {
		m_set.append (key);
	}
}

void KeySetVisitor::visit (TreeModel * model)
{
	TreeItem* item = model -> root();
	for (int i = 0; i < item->childCount (); i++) {
		item->child (i)->accept (*this);
	}
}

KeySet KeySetVisitor::getKeySet ()
{
	return m_set.dup ();
}

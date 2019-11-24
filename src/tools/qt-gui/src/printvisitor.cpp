/**
 * @file
 *
 * @brief
 *
 * @copyright BSD License (see LICENSE.md or https://www.libelektra.org)
 */

#include "printvisitor.hpp"

#include "confignode.hpp"
#include "treeviewmodel.hpp"
#include "treemodel.hpp"

void PrintVisitor::visit (ConfigNode & node)
{
	QStringList path = node.getPath ().split ("/");
	QString name;

		foreach (QString s, path)
			name += " ";

	name += node.getName ();

	std::cout << name.toStdString () << std::endl;
}

void PrintVisitor::visit (TreeViewModel * model)
{
		foreach (ConfigNodePtr node, model->model ())
		{
			node->accept (*this);
		}
}

void PrintVisitor::visit (TreeItem & item)
{
	QStringList path = item.path().split ("/");
	QString name;

	for (auto s: path) {
		name += " ";
	}

	name += item.name ();

	std::cout << name.toStdString () << std::endl;
}

void PrintVisitor::visit (TreeModel * model)
{
	TreeItem* item = model -> root();
	for (int i = 0; i < item->childCount (); i++) {
		item->child (i)->accept (*this);
	}
}

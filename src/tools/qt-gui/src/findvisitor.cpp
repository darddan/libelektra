/**
 * @file
 *
 * @brief
 *
 * @copyright BSD License (see LICENSE.md or https://www.libelektra.org)
 */

#include "findvisitor.hpp"
#include "treeviewmodel.hpp"
#include "treemodel.hpp"

FindVisitor::FindVisitor (TreeViewModel * searchResults, QString term) : m_searchResults (searchResults), m_term (std::move (term))
{
}

void FindVisitor::visit (ConfigNode & node)
{
	bool termFound = false;

	if (node.getPath ().contains (m_term) || node.getValue ().toString ().contains (m_term))
	{
		termFound = true;
	}

	if (node.getMetaKeys () && !termFound)
	{
		foreach (ConfigNodePtr metaNode, node.getMetaKeys ()->model ())
		{
			if (metaNode->getName ().contains (m_term) || metaNode->getValue ().toString ().contains (m_term))
			{
				termFound = true;
				break;
			}
		}
	}

	if (termFound)
		// let the other model delete this node
		m_searchResults->insertRow (m_searchResults->rowCount (), ConfigNodePtr (&node, &ConfigNode::dontDelete), false);
}

void FindVisitor::visit (TreeViewModel * model)
{
		foreach (ConfigNodePtr node, model->model ())
		{
			node->accept (*this);
		}
}

void FindVisitor::visit (TreeItem & item)
{
	bool termFound = false;

	if (item.path ().contains (m_term) || QString::fromStdString (item.key ().getString ()).contains (m_term))
	{
		termFound = true;
	}

	// TODO: Adjust the following part of the code
//	if (item.getMetaKeys () && !termFound)
//	{
//			foreach (ConfigNodePtr metaNode, node.getMetaKeys ()->model ())
//			{
//				if (metaNode->getName ().contains (m_term) || metaNode->getValue ().toString ().contains (m_term))
//				{
//					termFound = true;
//					break;
//				}
//			}
//	}

//	if (termFound) {
		// TODO: A better way to get the pointer of the key, or to create a ConfigNodePtr
//		kdb::Key key = item.key ();
//		auto configNodePtr = std::make_shared<ConfigNode> ((item.name (), item.path (), &key, nullptr));
		// let the other model delete this node
//		m_searchResults->insertRow (m_searchResults->rowCount (), configNodePtr , false);
//	}

}

void FindVisitor::visit (TreeModel * model)
{
	TreeItem* item = model -> root();
	for (int i = 0; i < item->childCount (); i++) {
		item->child (i)->accept (*this);
	}
}

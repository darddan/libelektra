/**
 * @file
 *
 * @brief
 *
 * @copyright BSD License (see LICENSE.md or https://www.libelektra.org)
 */

#ifndef VISITOR_H
#define VISITOR_H

#include <QSharedPointer>

class ConfigNode;
class TreeViewModel;
class TreeItem;
class TreeModel;

/**
 * @brief  The abstract Visitor class to support the visitor pattern.
 */
class Visitor
{

public:
	/**
	 * @brief The abstract method a visitor who wants to visit a ConfigNode needs to implement.
	 *
	 * @param node The visited ConfigNode
	 */
	virtual void visit (ConfigNode & node) = 0;

	/**
	 * @brief The abstract method a visitor who wants to visit a TreeViewModel needs to implement.
	 *
	 * @param model The visited TreeViewModel
	 */
	virtual void visit (TreeViewModel * model) = 0;

	/**
	 * @brief The abstract method a visitor who wants to visit a TreeItem needs to implement.
	 *
	 * @param node The visited TreeItem
	 */
	virtual void visit (TreeItem & item) = 0;

	/**
	 * @brief The abstract method a visitor who wants to visit a TreeModel needs to implement.
	 *
	 * @param model The visited TreeModel
	 */
	virtual void visit (TreeModel * model) = 0;
};

#endif // VISITOR_H

#ifndef ELEKTRA_TREEVIEWITEM_HPP
#define ELEKTRA_TREEVIEWITEM_HPP

#include <kdb.hpp>
#include "keysetvisitor.hpp"

#include <QString>
#include <QVariant>

class TreeItem
{
public:
	explicit TreeItem (const kdb::Key & key, TreeItem * parentItem = nullptr);
	~TreeItem ();

	QString name() const;
	QString path() const;
	kdb::Key key() const;
	bool expandable() const;

	void setExpandable(bool isExpandable);

	void appendChild (TreeItem * child);
	TreeItem * child (int row);
	int childCount () const;
	int columnCount () const;
	QVariant data (int column) const;
	int row () const;
	TreeItem * parentItem ();

	/**
	 * @brief The method thats accepts a Visitor object to support the Vistor Pattern.
	 * @param visitor The visitor that visits this TreeViewModel.
	 */
	void accept (Visitor & visitor);

private:
	QVector<TreeItem *> m_childItems;
	TreeItem * m_parentItem;
	kdb::Key m_key;

	QString m_name;
	QString m_path;
	bool m_isExpandable;
};

#endif // ELEKTRA_TREEVIEWITEM_HPP

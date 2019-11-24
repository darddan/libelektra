#ifndef ELEKTRA_TREEMODEL_HPP
#define ELEKTRA_TREEMODEL_HPP

#include "treeitem.hpp"
#include "keysetvisitor.hpp"

#include <QAbstractItemModel>
#include <QObject>
#include <QVariant>
#include <kdb.hpp>
#include <merging/mergingkdb.hpp>

class TreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum TreeViewModelRoles
	{
		NameRole = Qt::UserRole + 1, ///< The role QML can access the name of a ConfigNode at a specified index.
		PathRole,		     ///< The role QML can access the path of a ConfigNode at a specified index.
		ValueRole,		     ///< The role QML can access the value of a ConfigNode at a specified index.
		ChildCountRole,		     ///< The role QML can access the number of children of a ConfigNode at a specified index.
		ChildrenRole,		     ///< The role QML can access the children model of a ConfigNode at a specified index.
		ChildrenHaveNoChildrenRole,  ///< The role QML can access if children of a ConfigNode at a specified index do have children
		/// on their own.
			MetaValueRole,   ///< The role QML can access the meta model of a ConfigNode at a specified index.
		NodeRole,	///< The role QML can retrieve the ConfigNode at a specified index.
		ParentModelRole, ///< The role QML can retrieve a pointer to the ParentModel of a ConfigNode.
		IndexRole,       ///< The role QML can retrieve the index of a ConfigNode.
		IsNullRole,      ///< The role QML can retrieve if the key of this node is null.
		IsExpandedRole   ///< The role QML can retrieve if a ConfigNode is expanded.
	};

	explicit TreeModel(kdb::tools::merging::MergingKDB * kdb, QObject * parentModel = nullptr);
	~TreeModel();

	/* QAbstractItemModel interface */
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
			    int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column,
			  const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;


	TreeItem* root() const;


	Q_INVOKABLE void refresh ();
	Q_INVOKABLE void synchronize ();
	Q_INVOKABLE void exportConfiguration (const QModelIndex &parent, QString format, QString file);

	/**
	 * @brief Stores the current state of the configuration in the KeySet.
	 */
	kdb::KeySet collectCurrentKeySet ();

	/**
	 * @brief The method thats accepts a Visitor object to support the Vistor Pattern.
	 * @param visitor The visitor that visits this TreeViewModel.
	 */
	void accept (Visitor & visitor);

signals: // Use "Error", "Warning" and "Information" as title to display the according icon
	/**
	 * @brief Triggers a messagedialog in the GUI.
	 * @param title The title of the messagedialog in the GUI.
	 * @param text The text of the messagedialog in the GUI.This is the text that will be initially shown to the user.
	 * @param detailedText The detailed text of the messagedialog in the GUI.The user will have to click on a button to access this
	 * text.
	 */
	void showMessage (QString title, QString text, QString detailedText) const;
	/**
	 * @brief Triggers the expanded property of a ConfigNode.
	 */
	void expandNode (bool);
	/**
	 * @brief Triggers the update of the treeview in the GUI.
	 */
	void updateIndicator () const;
private:
	void setupModelData();

	TreeItem * m_rootItem;

	QHash<int, QByteArray> m_roleNameMapping;
	kdb::tools::merging::MergingKDB * m_kdb;
};

#endif // ELEKTRA_TREEMODEL_HPP

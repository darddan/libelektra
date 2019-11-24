import QtQuick 2.2
import QtQuick.Dialogs 1.1

FileDialog {

	title: qsTr("Export Configuration to File")
	selectExisting: false

	onAccepted: {
		var plugin = selectedNameFilter.match(/[a-z]+/).toString()

		treeView.model.exportConfiguration(treeView.currentIndex, plugin, exportDialog.fileUrl)
	}

}

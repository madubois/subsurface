// SPDX-License-Identifier: GPL-2.0
import QtQuick 2.5
import QtLocation 5.3
import QtPositioning 5.3
import org.subsurfacedivelog.mobile 1.0

Item {
	id: rootItem
	property alias mapHelper: mapHelper
	property alias map: map
	readonly property bool siteImageVisible: mapHelper.siteImageUrl.toString().length > 0

	signal selectedDivesChanged(var list)

	MapWidgetHelper {
		id: mapHelper
		map: map
		editMode: false
		onSelectedDivesChanged: rootItem.selectedDivesChanged(list)
		onEditModeChanged: editMessage.isVisible = editMode === true ? 1 : 0
		onCoordinatesChanged: {}
		Component.onCompleted: {
			map.plugin = Qt.createQmlObject(pluginObject, rootItem)
			map.mapType = { "STREET": map.supportedMapTypes[0], "SATELLITE": map.supportedMapTypes[1] }
			map.activeMapType = map.mapType.SATELLITE
		}
	}

	Map {
		id: map
		anchors.fill: parent
		zoomLevel: defaultZoomIn
		visible: !rootItem.siteImageVisible

		property var mapType
		readonly property var defaultCenter: QtPositioning.coordinate(0, 0)
		readonly property real defaultZoomIn: 12.0
		readonly property real defaultZoomOut: 1.0
		readonly property real textVisibleZoom: 11.0
		readonly property real zoomStep: 2.0
		property var newCenter: defaultCenter
		property real newZoom: 1.0
		property real newZoomOut: 1.0
		property var clickCoord: QtPositioning.coordinate(0, 0)
		property bool isReady: false

		Component.onCompleted: isReady = true
		onZoomLevelChanged: {
			if (isReady)
				mapHelper.calculateSmallCircleRadius(map.center)
		}

		MapItemView {
			id: mapItemView
			model: mapHelper.model
			delegate: MapQuickItem {
				id: mapItem
				anchorPoint.x: 0
				anchorPoint.y: mapItemImage.height
				coordinate:  model.coordinate
				z: model.z
				sourceItem: Image {
					id: mapItemImage
					source: model.pixmap
					SequentialAnimation {
						id: mapItemImageAnimation
						PropertyAnimation { target: mapItemImage; property: "scale"; from: 1.0; to: 0.7; duration: 120 }
						PropertyAnimation { target: mapItemImage; property: "scale"; from: 0.7; to: 1.0; duration: 80 }
					}
					MouseArea {
						drag.target: (mapHelper.editMode && model.isSelected) ? mapItem : undefined
						anchors.fill: parent
						onClicked: {
							if (!mapHelper.editMode && model.divesite)
								mapHelper.selectedLocationChanged(model.divesite)
						}
						onDoubleClicked: map.doubleClickHandler(mapItem.coordinate)
						onReleased: {
							if (mapHelper.editMode && model.isSelected) {
								mapHelper.updateCurrentDiveSiteCoordinatesFromMap(model.divesite, mapItem.coordinate)
							}
						}
					}
					Item {
						// Text with a duplicate for shadow. DropShadow as layer effect is kind of slow here.
						y: mapItemImage.y + mapItemImage.height
						visible: map.zoomLevel >= map.textVisibleZoom
						Text {
							id: mapItemTextShadow
							x: mapItemText.x + 2; y: mapItemText.y + 2
							text: mapItemText.text
							font.pointSize: mapItemText.font.pointSize
							color: "black"
						}
						Text {
							id: mapItemText
							text: model.name
							font.pointSize: 11.0
							color: model.isSelected ? "white" : "lightgrey"
						}
					}
				}
			}
		}

		SequentialAnimation {
			id: mapAnimationZoomIn
			NumberAnimation {
				target: map; property: "zoomLevel"; to: map.newZoomOut; duration: Math.abs(map.newZoomOut - map.zoomLevel) * 200
			}
			ParallelAnimation {
				CoordinateAnimation { target: map; property: "center"; to: map.newCenter; duration: 2000; easing.type: Easing.OutCubic }
				NumberAnimation {
					target: map; property: "zoomLevel"; to: map.newZoom; duration: 2000
				}
			}
		}

		ParallelAnimation {
			id: mapAnimationClick
			CoordinateAnimation { target: map; property: "center"; to: map.newCenter; duration: 500	}
			NumberAnimation { target: map; property: "zoomLevel"; to: map.newZoom; duration: 500 }
		}

		MouseArea {
			anchors.fill: parent
			onPressed: { map.stopZoomAnimations(); mouse.accepted = false }
			onWheel: { map.stopZoomAnimations(); wheel.accepted = false }
			onDoubleClicked: map.doubleClickHandler(map.toCoordinate(Qt.point(mouseX, mouseY)))
		}

		function doubleClickHandler(coord) {
			newCenter = coord
			newZoom = zoomLevel + zoomStep
			if (newZoom > maximumZoomLevel)
				newZoom = maximumZoomLevel
			mapAnimationClick.restart()
		}

		function pointIsVisible(pt) {
			return !isNaN(pt.x)
		}

		function coordIsValid(coord) {
			if (coord == null || isNaN(coord.latitude) || isNaN(coord.longitude) ||
			    (coord.latitude === 0.0 && coord.longitude === 0.0))
				return false;
			return true;
		}

		function stopZoomAnimations() {
			mapAnimationZoomIn.stop()
		}

		function centerOnCoordinate(coord) {
			stopZoomAnimations()
			if (!coordIsValid(coord)) {
				console.warn("MapWidget.qml: centerOnCoordinate(): !coordIsValid()")
				return
			}
			var newZoomOutFound = false
			var zoomStored = zoomLevel
			var centerStored = QtPositioning.coordinate(center.latitude, center.longitude)
			newZoomOut = zoomLevel
			newCenter = coord
			zoomLevel = Math.floor(zoomLevel)
			while (zoomLevel > minimumZoomLevel) {
				var pt = fromCoordinate(coord)
				if (pointIsVisible(pt)) {
					newZoomOut = zoomLevel
					newZoomOutFound = true
					break
				}
				zoomLevel -= 1.0
			}
			if (!newZoomOutFound)
				newZoomOut = defaultZoomOut
			zoomLevel = zoomStored
			center = centerStored
			newZoom = zoomStored
			mapAnimationZoomIn.restart()
		}

		function centerOnRectangle(topLeft, bottomRight, centerRect) {
			stopZoomAnimations()
			if (newCenter.latitude === 0.0 && newCenter.longitude === 0.0) {
				// Do nothing
				return
			}
			var centerStored = QtPositioning.coordinate(center.latitude, center.longitude)
			var zoomStored = zoomLevel
			var newZoomOutFound = false
			newCenter = centerRect
			// calculate zoom out
			newZoomOut = zoomLevel
			while (zoomLevel > minimumZoomLevel) {
				var ptCenter = fromCoordinate(centerStored)
				var ptCenterRect = fromCoordinate(centerRect)
				if (pointIsVisible(ptCenter) && pointIsVisible(ptCenterRect)) {
					newZoomOut = zoomLevel
					newZoomOutFound = true
					break
				}
				zoomLevel -= 1.0
			}
			if (!newZoomOutFound)
				newZoomOut = defaultZoomOut
			// calculate zoom in
			center = newCenter
			zoomLevel = Math.floor(maximumZoomLevel)
			var diagonalRect = topLeft.distanceTo(bottomRight)
			while (zoomLevel > minimumZoomLevel) {
				var c0 = toCoordinate(Qt.point(0.0, 0.0))
				var c1 = toCoordinate(Qt.point(width, height))
				if (c0.distanceTo(c1) > diagonalRect) {
					newZoom = zoomLevel - 2.0
					break
				}
				zoomLevel -= 1.0
			}
			if (newZoom > defaultZoomIn)
				newZoom = defaultZoomIn
			zoomLevel = zoomStored
			center = centerStored
			mapAnimationZoomIn.restart()
		}

		function deselectMapLocation() {
			stopZoomAnimations()
		}
	}

	Rectangle {
		id: editMessage
		radius: padding
		color: "#b08000"
		border.color: "white"
		x: (map.width - width) * 0.5; y: padding
		width: editMessageText.width + padding * 2.0
		height: editMessageText.height + padding * 2.0
		visible: false
		opacity: 0.0
		property int isVisible: -1
		property real padding: 10.0
		onOpacityChanged: visible = !rootItem.siteImageVisible && opacity != 0.0
		states: [
			State { when: editMessage.isVisible === 1; PropertyChanges { target: editMessage; opacity: 1.0 }},
			State { when: editMessage.isVisible === 0; PropertyChanges { target: editMessage; opacity: 0.0 }}
		]
		transitions: Transition { NumberAnimation { properties: "opacity"; easing.type: Easing.InOutQuad }}
		Connections {
			target: rootItem
			onSiteImageVisibleChanged: editMessage.visible = !rootItem.siteImageVisible && editMessage.opacity != 0.0
		}
		Text {
			id: editMessageText
			y: editMessage.padding; x: editMessage.padding
			verticalAlignment: Text.AlignVCenter
			color: "white"
			font.pointSize: 11.0
			text: qsTr("Drag the selected dive location")
		}
	}

	Image {
		id: toggleImage
		x: 10; y: x
		width: 40
		height: 40
		source: "qrc:///map-style-" + (map.activeMapType === map.mapType.SATELLITE ? "map" : "photo") + "-icon"
		visible: !rootItem.siteImageVisible
		SequentialAnimation {
			id: toggleImageAnimation
			PropertyAnimation { target: toggleImage; property: "scale"; from: 1.0; to: 0.8; duration: 120 }
			PropertyAnimation { target: toggleImage; property: "scale"; from: 0.8; to: 1.0; duration: 80 }
		}
		MouseArea {
			anchors.fill: parent
			onClicked: {
				map.activeMapType = map.activeMapType === map.mapType.SATELLITE ? map.mapType.STREET : map.mapType.SATELLITE
				toggleImageAnimation.restart()
			}
		}
	}

	Image {
		id: imageZoomIn
		x: 10 + (toggleImage.width - imageZoomIn.width) * 0.5; y: toggleImage.y + toggleImage.height + 10
		width: 20
		height: 20
		source: "qrc:///zoom-in-icon"
		visible: !rootItem.siteImageVisible
		SequentialAnimation {
			id: imageZoomInAnimation
			PropertyAnimation { target: imageZoomIn; property: "scale"; from: 1.0; to: 0.8; duration: 120 }
			PropertyAnimation { target: imageZoomIn; property: "scale"; from: 0.8; to: 1.0; duration: 80 }
		}
		MouseArea {
			anchors.fill: parent
			onClicked: {
				map.stopZoomAnimations()
				map.newCenter = map.center
				map.newZoom = map.zoomLevel + map.zoomStep
				if (map.newZoom > map.maximumZoomLevel)
					map.newZoom = map.maximumZoomLevel
				mapAnimationClick.restart()
				imageZoomInAnimation.restart()
			}
		}
	}

	Image {
		id: imageZoomOut
		x: imageZoomIn.x; y: imageZoomIn.y + imageZoomIn.height + 10
		source: "qrc:///zoom-out-icon"
		visible: !rootItem.siteImageVisible
		width: 20
		height: 20
		SequentialAnimation {
			id: imageZoomOutAnimation
			PropertyAnimation { target: imageZoomOut; property: "scale"; from: 1.0; to: 0.8; duration: 120 }
			PropertyAnimation { target: imageZoomOut; property: "scale"; from: 0.8; to: 1.0; duration: 80 }
		}
		MouseArea {
			anchors.fill: parent
			onClicked: {
				map.stopZoomAnimations()
				map.newCenter = map.center
				map.newZoom = map.zoomLevel - map.zoomStep
				mapAnimationClick.restart()
				imageZoomOutAnimation.restart()
			}
		}
	}

	/*
	 * open coordinates in google maps while attempting to roughly preserve
	 * the zoom level. the mapping between the QML map zoom level and the
	 * Google Maps zoom level is done via exponential regression:
	 *     y = a * exp(b * x)
	 *
	 * data set:
	 *     qml (x)            gmaps (y in meters)
	 *     21                 257
	 *     15.313216749178913 3260
	 *     12.553216749178931 20436
	 *     11.11321674917894  52883
	 *     9.313216749178952  202114
	 *     7.51321674917896   737136
	 *     5.593216749178958  2495529
	 *     4.153216749178957  3895765
	 *     1.753216749178955  18999949
	 */
	function openLocationInGoogleMaps(latitude, longitude) {
		var loc = latitude + "," + longitude
		var poi = latitude + "+" + longitude
		var x = map.zoomLevel
		var a = 53864950.831693
		var b = -0.60455861606547030630
		var zoom = Math.floor(a * Math.exp(b * x))
		var url = "https://www.google.com/maps/place/" + poi + "/@" + loc + "," + zoom + "m/data=!3m1!1e3!4m2!3m1!1s0x0:0x0"
		Qt.openUrlExternally(url)
		console.log("openLocationInGoogleMaps() map.zoomLevel: " + x + ", url: " + url)
	}

	Item {
		id: siteImage
		anchors.fill: parent
		visible: rootItem.siteImageVisible
		clip: true
		property string overlayMode: "none"
		property var overlayItems: []
		property var activeLine: null
		property int activeNoteIndex: -1
		property int selectedNoteIndex: -1
		property int imageTransformRevision: 0

		function imagePoint(x, y) {
			var point = siteImageContent.mapFromItem(siteImage, x, y)
			return { x: Math.max(0, Math.min(1, point.x / siteImageContent.width)),
				y: Math.max(0, Math.min(1, point.y / siteImageContent.height)) }
		}

		function saveOverlay() {
			mapHelper.saveDiveImageOverlay(JSON.stringify(overlayItems))
			overlayCanvas.requestPaint()
		}

		function noteAt(point) {
			for (var i = overlayItems.length - 1; i >= 0; --i) {
				var item = overlayItems[i]
				if (item.type !== "note")
					continue
				var dx = item.x - point.x
				var dy = item.y - point.y
				if (Math.sqrt(dx * dx + dy * dy) < 0.12)
					return i
			}
			return -1
		}

		function wrappedLines(text, context, maxWidth) {
			var result = []
			var paragraphs = text.split("\n")
			for (var p = 0; p < paragraphs.length; ++p) {
				var words = paragraphs[p].split(" ")
				var current = ""
				for (var w = 0; w < words.length; ++w) {
					var candidate = current.length ? current + " " + words[w] : words[w]
					if (current.length && context.measureText(candidate).width > maxWidth) {
						result.push(current)
						current = words[w]
					} else {
						current = candidate
					}
				}
				result.push(current)
			}
			return result.length ? result : [""]
		}

		function fittedNote(item, context, maxWidth, maxHeight) {
			var size = item.size || 20
			var lines = []
			var lineHeight = size * siteImageContent.scale
			for (var attempt = 0; attempt < 8; ++attempt) {
				context.font = "bold " + (size * siteImageContent.scale) + "px sans-serif"
				lines = siteImage.wrappedLines(item.text, context, maxWidth - 16)
				lineHeight = size * siteImageContent.scale
				var requiredHeight = lines.length * lineHeight
				var requiredWidth = 0
				for (var line = 0; line < lines.length; ++line)
					requiredWidth = Math.max(requiredWidth, context.measureText(lines[line]).width)
				if (requiredHeight <= maxHeight - 2 && requiredWidth <= maxWidth - 2 || size <= 2)
					break
				var heightScale = (maxHeight - 2) / requiredHeight
				var widthScale = (maxWidth - 2) / requiredWidth
				size = Math.max(2, size * Math.min(heightScale, widthScale))
			}
			return { size: size, lines: lines, lineHeight: lineHeight }
		}

		function requiredBoxHeight(item, size) {
			var context = overlayCanvas.getContext("2d")
			context.font = "bold " + (size * siteImageContent.scale) + "px sans-serif"
			var maxWidth = (item.boxWidth || 0.35) * siteImageContent.width * siteImageContent.scale
			var lines = wrappedLines(item.text, context, maxWidth - 16)
			return (lines.length * size * siteImageContent.scale + 2) /
				(siteImageContent.height * siteImageContent.scale)
		}

		function restoreOverlay() {
			overlayItems = []
			try {
				overlayItems = JSON.parse(mapHelper.diveImageOverlayData)
				for (var i = 0; i < overlayItems.length; ++i) {
					var item = overlayItems[i]
					if (item.type === "note" && item.autoHeight !== false) {
						item.boxHeight = requiredBoxHeight(item, item.size || 20)
						item.autoHeight = true
					}
				}
			} catch (error) {
				overlayItems = []
			}
			overlayCanvas.requestPaint()
		}

		Image {
			id: siteImageContent
			width: siteImage.width
			height: siteImage.height
			x: (siteImage.width - width) * 0.5
			y: (siteImage.height - height) * 0.5
			source: mapHelper.siteImageUrl
			fillMode: Image.PreserveAspectFit
			scale: 1.0
		}

		Canvas {
			id: overlayCanvas
			anchors.fill: siteImage
			z: 2
			onPaint: {
				var context = getContext("2d")
				context.clearRect(0, 0, width, height)
				context.lineWidth = 4
				context.strokeStyle = "#e53935"
				context.fillStyle = "#e53935"
				context.textBaseline = "middle"
				context.textAlign = "center"
				context.font = "bold 20px sans-serif"
				for (var i = 0; i < siteImage.overlayItems.length; ++i) {
					var item = siteImage.overlayItems[i]
					if (item.type === "line") {
						var start = siteImageContent.mapToItem(siteImage,
							item.x1 * siteImageContent.width, item.y1 * siteImageContent.height)
						var end = siteImageContent.mapToItem(siteImage,
							item.x2 * siteImageContent.width, item.y2 * siteImageContent.height)
						context.beginPath()
						context.moveTo(start.x, start.y)
						context.lineTo(end.x, end.y)
						context.stroke()
					} else if (item.type === "note") {
						var handle = noteHandles.itemAt(i)
						var liveSize = handle && handle.visible ? handle.previewSize : (item.size || 20)
						var notePoint = siteImageContent.mapToItem(siteImage,
							item.x * siteImageContent.width, item.y * siteImageContent.height)
						var liveWidth = handle && handle.visible ? handle.previewWidth : (item.boxWidth || 0.35)
						var liveHeight = handle && handle.visible ? handle.previewHeight : (item.boxHeight || 0.2)
						var renderItem = { text: item.text, size: liveSize }
						var boxWidth = liveWidth * siteImageContent.width * siteImageContent.scale
						var boxHeight = liveHeight * siteImageContent.height * siteImageContent.scale
						var fitted = siteImage.fittedNote(renderItem, context, boxWidth, boxHeight)
						var lines = fitted.lines
						var lineHeight = fitted.lineHeight
						context.save()
						context.beginPath()
						context.rect(notePoint.x - boxWidth * 0.5, notePoint.y - boxHeight * 0.5, boxWidth, boxHeight)
						context.clip()
						for (var line = 0; line < lines.length; ++line)
							context.fillText(lines[line], notePoint.x,
								notePoint.y + (line - (lines.length - 1) * 0.5) * lineHeight)
						context.restore()
					}
				}
			}
		}

		Repeater {
			id: noteHandles
			model: siteImage.overlayItems
			delegate: Item {
				id: noteItem
				property bool isNote: modelData.type === "note"
				property real previewSize: modelData.size || 20
				property int lineCount: modelData.text.split("\n").length
				property real previewWidth: modelData.boxWidth || 0.35
				property real previewHeight: modelData.boxHeight || 0.2
				property real resizeStartWidth: previewWidth
				property real resizeStartHeight: previewHeight
				property real resizeStartMouseY: 0
				property real resizeStartSize: 20
				property real resizeStartMouseX: 0
				property var notePosition: {
					var revision = siteImage.imageTransformRevision
					return siteImageContent.mapToItem(siteImage,
						modelData.x * siteImageContent.width, modelData.y * siteImageContent.height)
				}
				visible: isNote && siteImage.overlayMode === "move"
				enabled: visible
				width: previewWidth * siteImageContent.width * siteImageContent.scale
				height: previewHeight * siteImageContent.height * siteImageContent.scale
				x: notePosition.x - width * 0.5
				y: notePosition.y - height * 0.5
				z: 3

				Text {
					id: noteText
					anchors.centerIn: parent
					text: modelData.text
					color: "transparent"
					font.bold: true
					font.pixelSize: noteItem.previewSize * siteImageContent.scale
				}

				Rectangle {
					anchors.fill: parent
					color: "transparent"
					border.color: "#e53935"
					border.width: 2
				}

				MouseArea {
					anchors.fill: parent
					drag.target: noteItem
					preventStealing: true
					cursorShape: Qt.OpenHandCursor
					onClicked: siteImage.selectedNoteIndex = index
					onReleased: {
					var point = siteImageContent.mapFromItem(siteImage,
						noteItem.x + noteItem.width * 0.5, noteItem.y + noteItem.height * 0.5)
					var updated = siteImage.overlayItems.slice(0)
					updated[index] = { type: "note", text: modelData.text, size: modelData.size || 20,
						boxWidth: noteItem.previewWidth, boxHeight: noteItem.previewHeight, autoHeight: false,
						x: Math.max(0, Math.min(1, point.x / siteImageContent.width)),
						y: Math.max(0, Math.min(1, point.y / siteImageContent.height)) }
					siteImage.overlayItems = updated
					siteImage.saveOverlay()
					}
				}

				Rectangle {
					width: 12
					height: 12
					x: parent.width - width
					y: parent.height - height
					color: "#e53935"
					border.color: "white"
					MouseArea {
						anchors.fill: parent
						cursorShape: Qt.SizeFDiagCursor
						onPressed: {
							noteItem.resizeStartMouseX = mouseX
							noteItem.resizeStartMouseY = mouseY
							noteItem.resizeStartWidth = noteItem.previewWidth
							noteItem.resizeStartHeight = noteItem.previewHeight
						}
						onPositionChanged: {
							if (pressed) {
								noteItem.previewWidth = Math.max(0.01, noteItem.resizeStartWidth + (mouseX - noteItem.resizeStartMouseX) / (siteImageContent.width * siteImageContent.scale))
								noteItem.previewHeight = Math.max(0.01, noteItem.resizeStartHeight + (mouseY - noteItem.resizeStartMouseY) / (siteImageContent.height * siteImageContent.scale))
								overlayCanvas.requestPaint()
							}
						}
						onReleased: {
							var updated = siteImage.overlayItems.slice(0)
							var note = updated[index]
							updated[index] = { type: "note", text: note.text,
								size: Math.max(2, noteItem.previewSize),
								boxWidth: noteItem.previewWidth, boxHeight: noteItem.previewHeight, autoHeight: false,
								x: note.x, y: note.y }
							siteImage.overlayItems = updated
							siteImage.selectedNoteIndex = index
							siteImage.saveOverlay()
						}
					}
				}
			}
		}

		Connections {
			target: siteImageContent
			onXChanged: { siteImage.imageTransformRevision++; overlayCanvas.requestPaint() }
			onYChanged: { siteImage.imageTransformRevision++; overlayCanvas.requestPaint() }
			onScaleChanged: { siteImage.imageTransformRevision++; overlayCanvas.requestPaint() }
			onWidthChanged: { siteImage.imageTransformRevision++; overlayCanvas.requestPaint() }
			onHeightChanged: { siteImage.imageTransformRevision++; overlayCanvas.requestPaint() }
		}

		property bool restoringViewState: false

		function restoreViewState() {
			var state = mapHelper.siteImageViewState()
			restoringViewState = true
			if (state.scale !== undefined) {
				siteImageContent.scale = state.scale
				siteImageContent.x = state.x
				siteImageContent.y = state.y
			} else {
				siteImageContent.scale = 1.0
				siteImageContent.x = (siteImage.width - siteImageContent.width) * 0.5
				siteImageContent.y = (siteImage.height - siteImageContent.height) * 0.5
			}
			restoringViewState = false
		}

		function saveViewState() {
			if (!restoringViewState)
				mapHelper.saveSiteImageViewState(siteImageContent.scale, siteImageContent.x, siteImageContent.y)
		}

		MouseArea {
			anchors.fill: parent
			drag.target: siteImage.overlayMode === "none" ? siteImageContent : undefined
			onPressed: {
				if (siteImage.overlayMode === "move")
					siteImage.activeNoteIndex = siteImage.noteAt(siteImage.imagePoint(mouseX, mouseY))
					siteImage.selectedNoteIndex = siteImage.activeNoteIndex
			}
			onClicked: {
				if (siteImage.overlayMode === "line") {
					var point = siteImage.imagePoint(mouseX, mouseY)
					if (!siteImage.activeLine) {
						siteImage.activeLine = point
					} else {
						siteImage.overlayItems.push({ type: "line",
							x1: siteImage.activeLine.x, y1: siteImage.activeLine.y,
							x2: point.x, y2: point.y })
						siteImage.activeLine = null
						siteImage.saveOverlay()
					}
					overlayCanvas.requestPaint()
				}
			}
			onPositionChanged: {
				if (siteImage.overlayMode === "move" && siteImage.activeNoteIndex >= 0 && pressed) {
					var point = siteImage.imagePoint(mouseX, mouseY)
					var updated = siteImage.overlayItems.slice(0)
					var note = updated[siteImage.activeNoteIndex]
					updated[siteImage.activeNoteIndex] = { type: "note", text: note.text, size: note.size || 20,
						x: point.x, y: point.y }
					siteImage.overlayItems = updated
					overlayCanvas.requestPaint()
				}
			}
			onReleased: {
				if (siteImage.overlayMode === "move") {
					if (siteImage.activeNoteIndex >= 0)
						siteImage.saveOverlay()
					siteImage.activeNoteIndex = -1
				}
				if (siteImage.overlayMode === "none")
					siteImage.saveViewState()
			}
			onWheel: {
				if (siteImage.overlayMode !== "none") {
					wheel.accepted = true
					return
				}
				var factor = wheel.angleDelta.y > 0 ? 1.2 : 1.0 / 1.2
				var nextScale = siteImageContent.scale * factor
				siteImageContent.scale = Math.max(1.0, Math.min(nextScale, 8.0))
				siteImage.saveViewState()
				wheel.accepted = true
			}
			onDoubleClicked: {
				if (siteImage.overlayMode !== "none")
					return
				siteImageContent.scale = siteImageContent.scale > 1.0 ? 1.0 : 2.0
				siteImageContent.x = (siteImage.width - siteImageContent.width) * 0.5
				siteImageContent.y = (siteImage.height - siteImageContent.height) * 0.5
				siteImage.saveViewState()
			}
		}

		Connections {
			target: mapHelper
			onSiteImagePathChanged: {
				siteImage.restoreOverlay()
				siteImage.restoreViewState()
			}
				onDiveImageOverlayChanged: siteImage.restoreOverlay()
				onCurrentDiveChanged: siteImage.restoreOverlay()
		}

		Component.onCompleted: restoreOverlay()

		Rectangle {
			id: overlayToolbar
			anchors.left: parent.left
			anchors.top: parent.top
			anchors.margins: 10
			width: overlayTools.width + 20
			height: 62
			color: "#cc202020"
			visible: rootItem.siteImageVisible

			Row {
				id: overlayTools
				anchors.centerIn: parent
				spacing: 6
				Rectangle { width: 58; height: 26; color: siteImage.overlayMode === "line" ? "#e53935" : "#555555"; Text { anchors.centerIn: parent; color: "white"; text: qsTr("Line") } MouseArea { anchors.fill: parent; onClicked: siteImage.overlayMode = siteImage.overlayMode === "line" ? "none" : "line" } }
				Rectangle { width: 58; height: 26; color: siteImage.overlayMode === "move" ? "#e53935" : "#555555"; Text { anchors.centerIn: parent; color: "white"; text: qsTr("Move") } MouseArea { anchors.fill: parent; onClicked: siteImage.overlayMode = siteImage.overlayMode === "move" ? "none" : "move" } }
				TextEdit { id: noteInput; width: 130; height: 50; color: "white"; font.pixelSize: 14; clip: true; selectByMouse: true; wrapMode: TextEdit.Wrap; text: qsTr("Note") }
				Rectangle { width: 58; height: 26; color: "#555555"; Text { anchors.centerIn: parent; color: "white"; text: qsTr("Add") } MouseArea { anchors.fill: parent; onClicked: { if (noteInput.text.length > 0) { var newSize = parseInt(noteSizeInput.text) || 20; var newNote = { type: "note", text: noteInput.text, size: newSize, boxWidth: 0.35, boxHeight: 0.2, autoHeight: true, x: 0.1, y: 0.15 }; newNote.boxHeight = siteImage.requiredBoxHeight(newNote, newSize); siteImage.overlayItems.push(newNote); siteImage.saveOverlay(); noteInput.text = "" } } } }
				TextInput { id: noteSizeInput; width: 45; height: 26; color: "white"; font.pixelSize: 14; verticalAlignment: TextInput.AlignVCenter; inputMethodHints: Qt.ImhDigitsOnly; text: "20" }
				Rectangle { width: 58; height: 26; color: "#555555"; Text { anchors.centerIn: parent; color: "white"; text: qsTr("Size") } MouseArea { anchors.fill: parent; onClicked: { var size = parseInt(noteSizeInput.text); if (siteImage.selectedNoteIndex >= 0 && !isNaN(size)) { size = Math.max(2, Math.min(96, size)); var updated = siteImage.overlayItems.slice(0); var note = updated[siteImage.selectedNoteIndex]; updated[siteImage.selectedNoteIndex] = { type: "note", text: note.text, size: size, boxWidth: note.boxWidth || 0.35, boxHeight: Math.max(note.boxHeight || 0.2, siteImage.requiredBoxHeight(note, size)), x: note.x, y: note.y }; siteImage.overlayItems = updated; siteImage.saveOverlay(); } } } }
				Rectangle { width: 58; height: 26; color: "#555555"; Text { anchors.centerIn: parent; color: "white"; text: qsTr("Clear") } MouseArea { anchors.fill: parent; onClicked: { siteImage.overlayItems = []; siteImage.saveOverlay() } } }
			}
		}

		Rectangle {
			id: imageOverlay
			anchors.top: parent.top
			anchors.right: parent.right
			anchors.margins: 10
			width: clearImageButton.width + 20
			height: clearImageButton.height + 20
			color: "#b08000"
			radius: 5
			visible: rootItem.siteImageVisible

			MouseArea {
				id: clearImageButton
				anchors.centerIn: parent
				width: clearText.width + 10
				height: clearText.height + 6

				Text {
					id: clearText
					anchors.centerIn: parent
					text: qsTr("Clear")
					color: "white"
					font.pointSize: 10
				}

				onClicked: mapHelper.clearSiteImage()

				SequentialAnimation {
					id: clearAnimation
					PropertyAnimation { target: imageOverlay; property: "scale"; from: 1.0; to: 0.9; duration: 100 }
					PropertyAnimation { target: imageOverlay; property: "scale"; from: 0.9; to: 1.0; duration: 80 }
				}

				onPressed: clearAnimation.restart()
			}
		}
	}

	MapWidgetContextMenu {
		id: contextMenu
		visible: !rootItem.siteImageVisible
		y: 10; x: map.width - y
		onActionSelected: {
			switch (action) {
			case contextMenu.actions.OPEN_LOCATION_IN_GOOGLE_MAPS:
				openLocationInGoogleMaps(map.center.latitude, map.center.longitude)
				break
			case contextMenu.actions.COPY_LOCATION_DECIMAL:
				mapHelper.copyToClipboardCoordinates(map.center, false)
				break
			case contextMenu.actions.COPY_LOCATION_SEXAGESIMAL:
				mapHelper.copyToClipboardCoordinates(map.center, true)
				break
			case contextMenu.actions.SELECT_VISIBLE_LOCATIONS:
				mapHelper.selectVisibleLocations()
				break
			}
		}
	}
}

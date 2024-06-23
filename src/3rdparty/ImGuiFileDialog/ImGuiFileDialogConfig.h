#pragma once

#include <CustomFont.h>

// uncomment and modify defines under for customize ImGuiFileDialog

// uncomment if you need to use your FileSystem Interface
// if commented, you have two defualt interface, std::filesystem or dirent
// #define USE_CUSTOM_FILESYSTEM

// this options need c++17
#define USE_STD_FILESYSTEM

//#define MAX_FILE_DIALOG_NAME_BUFFER 1024
//#define MAX_PATH_BUFFER_SIZE 1024

// the slash's buttons in path can be used for quick select parallel directories
#define USE_QUICK_PATH_SELECT

// the spacing between button path's can be customized.
// if disabled the spacing is defined by the imgui theme
// define the space between path buttons
#define CUSTOM_PATH_SPACING 2

//#define USE_THUMBNAILS
//the thumbnail generation use the stb_image and stb_resize lib who need to define the implementation
//btw if you already use them in your app, you can have compiler error due to "implementation found in double"
//so uncomment these line for prevent the creation of implementation of these libs again
//#define DONT_DEFINE_AGAIN__STB_IMAGE_IMPLEMENTATION
//#define DONT_DEFINE_AGAIN__STB_IMAGE_RESIZE_IMPLEMENTATION
//#define IMGUI_RADIO_BUTTON RadioButton
//#define DisplayMode_ThumbailsList_ImageHeight 32.0f
//#define tableHeaderFileThumbnailsString "Thumbnails"
#define DisplayMode_FilesList_ButtonString ICON_IGFD_FILE_LIST
//#define DisplayMode_FilesList_ButtonHelp "File List"
#define DisplayMode_ThumbailsList_ButtonString ICON_IGFD_FILE_LIST_THUMBNAILS
//#define DisplayMode_ThumbailsList_ButtonHelp "Thumbnails List"
#define DisplayMode_ThumbailsGrid_ButtonString ICON_IGFD_FILE_GRID_THUMBNAILS
//#define DisplayMode_ThumbailsGrid_ButtonHelp "Thumbnails Grid"

#define USE_EXPLORATION_BY_KEYS
// this mapping by default is for GLFW but you can use another
//#include <GLFW/glfw3.h>
// Up key for explore to the top
//#define IGFD_KEY_UP ImGuiKey_UpArrow
// Down key for explore to the bottom
//#define IGFD_KEY_DOWN ImGuiKey_DownArrow
// Enter key for open directory
//#define IGFD_KEY_ENTER ImGuiKey_Enter
// BackSpace for coming back to the last directory
//#define IGFD_KEY_BACKSPACE ImGuiKey_Backspace

// by ex you can quit the dialog by pressing the key escape
//#define USE_DIALOG_EXIT_WITH_KEY
//#define IGFD_EXIT_KEY ImGuiKey_Escape

// widget
// begin combo widget
// #define IMGUI_BEGIN_COMBO ImGui::BeginCombo
// when auto resized, FILTER_COMBO_MIN_WIDTH will be considered has minimum width
// FILTER_COMBO_AUTO_SIZE is enabled by default now to 1
// uncomment if you want disable
//#define FILTER_COMBO_AUTO_SIZE 0
// filter combobox width
//#define FILTER_COMBO_MIN_WIDTH 120.0f
// button widget use for compose path
//#define IMGUI_PATH_BUTTON ImGui::Button
// standard button
//#define IMGUI_BUTTON ImGui::Button

// locales string
#define createDirButtonString ICON_IGFD_ADD
#define resetButtonString ICON_IGFD_UNDO
#define drivesButtonString ICON_IGFD_DRIVES
#define editPathButtonString ICON_IGFD_EDIT
#define searchString ICON_IGFD_FILTER
#define dirEntryString ICON_IGFD_FOLDER
#define linkEntryString ICON_IGFD_LINK
#define fileEntryString ICON_IGFD_FILE
//#define buttonResetSearchString "Reset filter"
//#define buttonDriveString "Drives"
//#define buttonEditPathString "Edit path\nYou can also right click on path buttons"
//#define buttonResetPathString "Reset to current directory"
//#define buttonCreateDirString "Create Directory"
//#define OverWriteDialogTitleString "The file Already Exist !"
//#define OverWriteDialogMessageString "Would you like to OverWrite it ?"
#define OverWriteDialogConfirmButtonString ICON_IGFD_OK " Confirm"
#define OverWriteDialogCancelButtonString ICON_IGFD_CANCEL " Cancel"

//Validation buttons
#define okButtonString ICON_IGFD_OK " OK"
#define okButtonWidth 100.0f
#define cancelButtonString ICON_IGFD_CANCEL " Cancel"
#define cancelButtonWidth 100.0f
//alignment [0:1], 0.0 is left, 0.5 middle, 1.0 right, and other ratios
#define okCancelButtonAlignement 0.0f
//#define invertOkAndCancelButtons 0

// DateTimeFormat
// see strftime function in <ctime> for customize
// "%Y/%m/%d %H:%M" give 2021:01:22 11:47
// "%Y/%m/%d %i:%M%p" give 2021:01:22 11:45PM
//#define DateTimeFormat "%Y/%m/%d %i:%M%p"

// theses icons will appear in table headers
#define USE_CUSTOM_SORTING_ICON
#define tableHeaderAscendingIcon ICON_IGFD_CHEVRON_UP
#define tableHeaderDescendingIcon ICON_IGFD_CHEVRON_DOWN
//#define tableHeaderFileNameString " File name"
//#define tableHeaderFileTypeString " Type"
//#define tableHeaderFileSizeString " Size"
//#define tableHeaderFileDateTimeString " Date"
#define fileSizeBytes "B"
#define fileSizeKiloBytes "kB"
#define fileSizeMegaBytes "MB"
#define fileSizeGigaBytes "GB"

// default table sort field (must be FIELD_FILENAME, FIELD_TYPE, FIELD_SIZE, FIELD_DATE or FIELD_THUMBNAILS)
//#define defaultSortField FIELD_FILENAME

// default table sort order for each field (true => Descending, false => Ascending)
//#define defaultSortOrderFilename true
//#define defaultSortOrderType true
//#define defaultSortOrderSize true
//#define defaultSortOrderDate true
//#define defaultSortOrderThumbnails true

#define USE_BOOKMARK
//#define bookmarkPaneWith 150.0f
//#define IMGUI_TOGGLE_BUTTON ToggleButton
#define bookmarksButtonString ICON_IGFD_BOOKMARK
//#define bookmarksButtonHelpString "Bookmark"
#define addBookmarkButtonString ICON_IGFD_ADD
#define removeBookmarkButtonString ICON_IGFD_REMOVE

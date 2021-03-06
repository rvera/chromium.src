// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/renderer_context_menu/render_view_context_menu_gtk.h"

#include <gtk/gtk.h>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "grit/generated_resources.h"
#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

namespace {

// A callback function for gtk_container_foreach(). This callback just checks
// the menu ID and set the given user data if it is same as the specified ID.
struct GtkWidgetAtParam {
  int index;
  GtkWidget* widget;
};

void GtkWidgetAt(GtkWidget* widget, gpointer user_data) {
  GtkWidgetAtParam* param = reinterpret_cast<GtkWidgetAtParam*>(user_data);

  gpointer data = g_object_get_data(G_OBJECT(widget), "menu-id");
  if (data && (GPOINTER_TO_INT(data) - 1) == param->index &&
      GTK_IS_MENU_ITEM(widget)) {
    param->widget = widget;
  }
}

// Retrieves a GtkWidget which has the specified command_id. This function
// traverses the given |model| in the depth-first order. When this function
// finds an item whose command_id is the same as the given |command_id|, it
// returns the GtkWidget associated with the item. This function emulates
// views::MenuItemViews::GetMenuItemByID() for GTK.
GtkWidget* GetMenuItemByID(ui::MenuModel* model,
                           GtkWidget* menu,
                           int command_id) {
  if (!menu)
    return NULL;

  for (int i = 0; i < model->GetItemCount(); ++i) {
    if (model->GetCommandIdAt(i) == command_id) {
      GtkWidgetAtParam param;
      param.index = i;
      param.widget = NULL;
      gtk_container_foreach(GTK_CONTAINER(menu), &GtkWidgetAt, &param);
      return param.widget;
    }

    ui::MenuModel* submenu = model->GetSubmenuModelAt(i);
    if (submenu) {
      GtkWidget* subitem = GetMenuItemByID(
          submenu,
          gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu)),
          command_id);
      if (subitem)
        return subitem;
    }
  }
  return NULL;
}

}  // namespace

RenderViewContextMenuGtk::RenderViewContextMenuGtk(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params,
    content::RenderWidgetHostView* view)
    : RenderViewContextMenu(render_frame_host, params),
      bidi_submenu_model_(this) {
  GdkEventButton* event = view->GetLastMouseDown();
  triggering_event_time_ = event ? event->time : GDK_CURRENT_TIME;
}

RenderViewContextMenuGtk::~RenderViewContextMenuGtk() {
}

void RenderViewContextMenuGtk::PlatformInit() {
  menu_gtk_.reset(new MenuGtk(this, &menu_model_));

  if (params_.is_editable) {
    content::RenderWidgetHostView* rwhv =
        source_web_contents_->GetRenderWidgetHostView();
    if (rwhv) {
      MenuGtk* menu = menu_gtk_.get();
      gboolean show_input_method_menu = TRUE;

      g_object_get(
          gtk_widget_get_settings(GTK_WIDGET(rwhv->GetNativeView())),
          "gtk-show-input-method-menu", &show_input_method_menu, NULL);
      if (!show_input_method_menu)
        return;

      std::string label = ui::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_INPUT_METHODS_MENU));
      GtkWidget* menuitem = gtk_menu_item_new_with_mnemonic(label.c_str());
      GtkWidget* submenu = rwhv->BuildInputMethodsGtkMenu();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
      int inspect_element_index =
          menu_model_.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_INSPECTELEMENT);
      if (inspect_element_index != -1) {
        menu->InsertSeparator(inspect_element_index - 1);
        menu->InsertMenuItem(IDC_INPUT_METHODS_MENU, menuitem,
            inspect_element_index);
      } else {
        menu->AppendSeparator();
        menu->AppendMenuItem(IDC_INPUT_METHODS_MENU, menuitem);
      }
    }
  }
}

void RenderViewContextMenuGtk::PlatformCancel() {
  menu_gtk_->Cancel();
}

bool RenderViewContextMenuGtk::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void RenderViewContextMenuGtk::Popup(const gfx::Point& point) {
  menu_gtk_->PopupAsContext(point, triggering_event_time_);
}

bool RenderViewContextMenuGtk::AlwaysShowIconForCmd(int command_id) const {
  return command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST;
}

void RenderViewContextMenuGtk::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      // WebKit's current behavior is for this menu item to always be disabled.
      NOTREACHED();
      break;

    case IDC_WRITING_DIRECTION_RTL:
    case IDC_WRITING_DIRECTION_LTR: {
      content::RenderViewHost* view_host = GetRenderViewHost();
      blink::WebTextDirection dir = blink::WebTextDirectionLeftToRight;
      if (command_id == IDC_WRITING_DIRECTION_RTL)
        dir = blink::WebTextDirectionRightToLeft;
      view_host->UpdateTextDirection(dir);
      view_host->NotifyTextDirection();
      break;
    }

    default:
      RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
      break;
  }
}

bool RenderViewContextMenuGtk::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_DEFAULT:
      return params_.writing_direction_default &
          blink::WebContextMenuData::CheckableMenuItemChecked;
    case IDC_WRITING_DIRECTION_RTL:
      return params_.writing_direction_right_to_left &
          blink::WebContextMenuData::CheckableMenuItemChecked;
    case IDC_WRITING_DIRECTION_LTR:
      return params_.writing_direction_left_to_right &
          blink::WebContextMenuData::CheckableMenuItemChecked;

    default:
      return RenderViewContextMenu::IsCommandIdChecked(command_id);
  }
}

bool RenderViewContextMenuGtk::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDC_WRITING_DIRECTION_MENU:
      return true;
    case IDC_WRITING_DIRECTION_DEFAULT:  // Provided to match OS defaults.
      return params_.writing_direction_default &
          blink::WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_RTL:
      return params_.writing_direction_right_to_left &
          blink::WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_LTR:
      return params_.writing_direction_left_to_right &
          blink::WebContextMenuData::CheckableMenuItemEnabled;

    default:
      return RenderViewContextMenu::IsCommandIdEnabled(command_id);
  }
}

void RenderViewContextMenuGtk::AppendPlatformEditableItems() {
  // OS X and Linux provide a contextual menu to set writing direction for BiDi
  // languages.
  // This functionality is exposed as a keyboard shortcut on Windows.
  AppendBidiSubMenu();
}

void RenderViewContextMenuGtk::AppendBidiSubMenu() {
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_DEFAULT,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT));
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_LTR,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR));
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_RTL,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL));

  menu_model_.AddSubMenu(
      IDC_WRITING_DIRECTION_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU),
      &bidi_submenu_model_);
}

void RenderViewContextMenuGtk::UpdateMenuItem(int command_id,
                                              bool enabled,
                                              bool hidden,
                                              const base::string16& title) {
  GtkWidget* item = GetMenuItemByID(&menu_model_, menu_gtk_->widget(),
                                    command_id);
  if (!item || !GTK_IS_MENU_ITEM(item))
    return;

  // Enable (or disable) the menu item and updates its text.
  gtk_widget_set_sensitive(item, enabled);
  if (hidden)
    gtk_widget_hide(item);
  else
    gtk_widget_show(item);
  gtk_menu_item_set_label(GTK_MENU_ITEM(item),
                          base::UTF16ToUTF8(title).c_str());
}

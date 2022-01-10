#pragma once

// From Rack ui but modified to have more possibilities to customize.

#include "plugin.hpp"
struct MTextField : OpaqueWidget {
	std::string text;
	std::string placeholder;
	/** Masks text with "*". */
	bool password = false;
	bool multiline = false;
	/** The index of the text cursor */
	int cursor = 0;
	/** The index of the other end of the selection.
	If nothing is selected, this is equal to `cursor`.
	*/
	int selection = 0;

	/** For Tab and Shift-Tab focusing.
	*/
	widget::Widget* prevField = NULL;
  widget::Widget* nextField = NULL;
  ui::Menu* menu;
	MTextField();
	void draw(const DrawArgs& args) override;
	void onDragHover(const DragHoverEvent& e) override;
	void onButton(const ButtonEvent& e) override;
	void onSelectText(const SelectTextEvent& e) override;
	void onSelectKey(const SelectKeyEvent& e) override;
	virtual int getTextPosition(math::Vec mousePos);

	std::string getText();
	/** Replaces the entire text */
	void setText(std::string text);
	void selectAll();
	std::string getSelectedText();
	/** Inserts text at the cursor, replacing the selection if necessary */
	void insertText(std::string text);
	virtual void copyClipboard(bool menu = true);
	virtual void cutClipboard(bool menu = true);
	virtual void pasteClipboard(bool menu = true);
	void cursorToPrevWord();
	void cursorToNextWord();
	virtual void createContextMenu();
};

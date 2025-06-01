# ImGui Common Patterns & Examples

## Window Management

### Dockable Windows Layout

```cpp
class DockableLayout {
public:
    void setup() {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    
    void render() {
        // Create main dockspace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = 
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;
            
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);
        
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
        
        // First time setup
        static bool first_time = true;
        if (first_time) {
            first_time = false;
            
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
            
            ImGuiID dock_left = ImGui::DockBuilderSplitNode(dockspace_id, 
                ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dockspace_id, 
                ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
            ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dockspace_id, 
                ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);
            
            ImGui::DockBuilderDockWindow("Tools", dock_left);
            ImGui::DockBuilderDockWindow("Properties", dock_right);
            ImGui::DockBuilderDockWindow("Console", dock_bottom);
            ImGui::DockBuilderDockWindow("Viewport", dockspace_id);
            
            ImGui::DockBuilderFinish(dockspace_id);
        }
        
        ImGui::End();
    }
};
```

### Modal Dialogs

```cpp
class ModalManager {
private:
    bool show_confirm = false;
    bool show_error = false;
    std::string error_message;
    std::function<void()> confirm_callback;
    
public:
    void showConfirmDialog(const std::string& message, 
                          std::function<void()> on_confirm) {
        show_confirm = true;
        confirm_callback = on_confirm;
        ImGui::OpenPopup("Confirm");
    }
    
    void showErrorDialog(const std::string& message) {
        show_error = true;
        error_message = message;
        ImGui::OpenPopup("Error");
    }
    
    void render() {
        // Confirm dialog
        if (ImGui::BeginPopupModal("Confirm", &show_confirm, 
            ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure?");
            ImGui::Separator();
            
            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                if (confirm_callback) confirm_callback();
                ImGui::CloseCurrentPopup();
                show_confirm = false;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                show_confirm = false;
            }
            ImGui::EndPopup();
        }
        
        // Error dialog  
        if (ImGui::BeginPopupModal("Error", &show_error,
            ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
            ImGui::Text("Error!");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::TextWrapped("%s", error_message.c_str());
            ImGui::Separator();
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                show_error = false;
            }
            ImGui::EndPopup();
        }
    }
};
```

## Data Display

### Sortable Table

```cpp
class SortableTable {
private:
    enum Column { Name, Size, Date, COUNT };
    struct Item {
        std::string name;
        int size;
        std::time_t date;
    };
    
    std::vector<Item> items;
    ImGuiTableSortSpecs* sorts_specs = nullptr;
    
    static int CompareItems(const void* lhs, const void* rhs) {
        const Item* a = (const Item*)lhs;
        const Item* b = (const Item*)rhs;
        
        for (int n = 0; n < sorts_specs->SpecsCount; n++) {
            const ImGuiTableColumnSortSpecs* spec = &sorts_specs->Specs[n];
            int delta = 0;
            
            switch (spec->ColumnIndex) {
            case Name: delta = strcmp(a->name.c_str(), b->name.c_str()); break;
            case Size: delta = (a->size - b->size); break;
            case Date: delta = (a->date - b->date); break;
            }
            
            if (delta > 0)
                return (spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
            if (delta < 0)
                return (spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
        }
        return 0;
    }
    
public:
    void render() {
        if (ImGui::BeginTable("Files", Column::COUNT,
            ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable)) {
            
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_PreferSortDescending);
            ImGui::TableSetupColumn("Date");
            ImGui::TableHeadersRow();
            
            // Sort data
            if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
                if (specs->SpecsDirty) {
                    sorts_specs = specs;
                    qsort(items.data(), items.size(), sizeof(Item), CompareItems);
                    specs->SpecsDirty = false;
                }
            }
            
            // Display data
            for (const auto& item : items) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", item.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d KB", item.size / 1024);
                ImGui::TableSetColumnIndex(2);
                char buf[32];
                strftime(buf, 32, "%Y-%m-%d", localtime(&item.date));
                ImGui::Text("%s", buf);
            }
            
            ImGui::EndTable();
        }
    }
};
```

### Live Plotting

```cpp
class LivePlotter {
private:
    static constexpr int HISTORY_SIZE = 1000;
    std::deque<float> values;
    float min_value = FLT_MAX;
    float max_value = -FLT_MAX;
    
public:
    void addValue(float value) {
        values.push_back(value);
        if (values.size() > HISTORY_SIZE) {
            values.pop_front();
        }
        
        // Update bounds
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }
    
    void render(const char* label, ImVec2 size = ImVec2(0, 80)) {
        if (values.empty()) return;
        
        // Convert to array for ImGui
        std::vector<float> arr(values.begin(), values.end());
        
        char overlay[32];
        sprintf(overlay, "%.2f", values.back());
        
        ImGui::PlotLines(label, arr.data(), arr.size(), 0, overlay,
            min_value - (max_value - min_value) * 0.1f,
            max_value + (max_value - min_value) * 0.1f,
            size);
            
        // Stats below
        ImGui::Text("Min: %.2f, Max: %.2f, Avg: %.2f",
            min_value, max_value,
            std::accumulate(values.begin(), values.end(), 0.0f) / values.size());
    }
};
```

## File Browser

```cpp
class FileBrowser {
private:
    std::filesystem::path current_path;
    std::string selected_file;
    std::vector<std::filesystem::directory_entry> entries;
    char search_buffer[256] = "";
    
    void refreshDirectory() {
        entries.clear();
        
        // Add parent directory
        if (current_path.has_parent_path()) {
            entries.push_back(std::filesystem::directory_entry(
                current_path.parent_path()));
        }
        
        // Add entries
        for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
            entries.push_back(entry);
        }
        
        // Sort: directories first, then alphabetical
        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) {
                    return a.is_directory();
                }
                return a.path().filename() < b.path().filename();
            });
    }
    
public:
    FileBrowser() : current_path(std::filesystem::current_path()) {
        refreshDirectory();
    }
    
    void render() {
        // Path bar
        ImGui::Text("Path: %s", current_path.string().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            refreshDirectory();
        }
        
        // Search
        ImGui::InputText("Search", search_buffer, sizeof(search_buffer));
        
        ImGui::Separator();
        
        // File list
        if (ImGui::BeginChild("FileList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()))) {
            for (const auto& entry : entries) {
                std::string name = entry.path().filename().string();
                
                // Filter by search
                if (strlen(search_buffer) > 0) {
                    if (name.find(search_buffer) == std::string::npos) {
                        continue;
                    }
                }
                
                // Icon based on type
                const char* icon = entry.is_directory() ? "📁" : "📄";
                
                if (ImGui::Selectable(
                    (icon + std::string(" ") + name).c_str(),
                    selected_file == entry.path().string())) {
                    
                    if (entry.is_directory()) {
                        current_path = entry.path();
                        refreshDirectory();
                    } else {
                        selected_file = entry.path().string();
                    }
                }
                
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (!entry.is_directory()) {
                        // Open file
                        onFileSelected(entry.path());
                    }
                }
            }
        }
        ImGui::EndChild();
        
        // Selected file
        ImGui::Text("Selected: %s", 
            selected_file.empty() ? "None" : selected_file.c_str());
    }
    
    std::function<void(const std::filesystem::path&)> onFileSelected;
};
```

## Property Inspector

```cpp
class PropertyInspector {
private:
    struct Property {
        std::string name;
        std::string category;
        std::function<void()> render;
    };
    
    std::vector<Property> properties;
    std::set<std::string> collapsed_categories;
    
public:
    template<typename T>
    void addProperty(const std::string& category, const std::string& name, T* value);
    
    void render() {
        std::string current_category;
        
        for (const auto& prop : properties) {
            // Category header
            if (prop.category != current_category) {
                current_category = prop.category;
                
                bool collapsed = collapsed_categories.count(current_category) > 0;
                
                ImGui::PushStyleColor(ImGuiCol_Header, 
                    ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                    
                if (ImGui::CollapsingHeader(current_category.c_str(),
                    collapsed ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
                    collapsed_categories.erase(current_category);
                } else {
                    collapsed_categories.insert(current_category);
                }
                
                ImGui::PopStyleColor();
                
                if (collapsed_categories.count(current_category) > 0) {
                    continue;
                }
            }
            
            // Property
            ImGui::PushID(prop.name.c_str());
            ImGui::Columns(2);
            ImGui::Text("%s", prop.name.c_str());
            ImGui::NextColumn();
            prop.render();
            ImGui::Columns(1);
            ImGui::PopID();
        }
    }
};

// Specializations
template<>
void PropertyInspector::addProperty(const std::string& category, 
                                   const std::string& name, 
                                   float* value) {
    properties.push_back({name, category, [value]() {
        ImGui::DragFloat("##value", value, 0.1f);
    }});
}

template<>
void PropertyInspector::addProperty(const std::string& category,
                                   const std::string& name,
                                   bool* value) {
    properties.push_back({name, category, [value]() {
        ImGui::Checkbox("##value", value);
    }});
}
```

## Console/Log Window

```cpp
class Console {
private:
    struct LogEntry {
        enum Type { Info, Warning, Error } type;
        std::string message;
        std::time_t timestamp;
    };
    
    std::vector<LogEntry> entries;
    ImGuiTextFilter filter;
    bool auto_scroll = true;
    bool show_timestamps = true;
    
public:
    void log(const std::string& message) {
        entries.push_back({LogEntry::Info, message, std::time(nullptr)});
    }
    
    void warn(const std::string& message) {
        entries.push_back({LogEntry::Warning, message, std::time(nullptr)});
    }
    
    void error(const std::string& message) {
        entries.push_back({LogEntry::Error, message, std::time(nullptr)});
    }
    
    void render() {
        // Controls
        if (ImGui::Button("Clear")) {
            entries.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &auto_scroll);
        ImGui::SameLine();
        ImGui::Checkbox("Timestamps", &show_timestamps);
        ImGui::SameLine();
        filter.Draw("Filter", -100.0f);
        
        ImGui::Separator();
        
        // Log display
        if (ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, 
            ImGuiWindowFlags_HorizontalScrollbar)) {
            
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            
            for (const auto& entry : entries) {
                if (!filter.PassFilter(entry.message.c_str()))
                    continue;
                
                // Color based on type
                ImVec4 color;
                const char* prefix;
                switch (entry.type) {
                    case LogEntry::Info:
                        color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                        prefix = "[INFO]";
                        break;
                    case LogEntry::Warning:
                        color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
                        prefix = "[WARN]";
                        break;
                    case LogEntry::Error:
                        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                        prefix = "[ERROR]";
                        break;
                }
                
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                
                if (show_timestamps) {
                    char time_buf[32];
                    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", 
                        localtime(&entry.timestamp));
                    ImGui::Text("[%s] %s %s", time_buf, prefix, 
                        entry.message.c_str());
                } else {
                    ImGui::Text("%s %s", prefix, entry.message.c_str());
                }
                
                ImGui::PopStyleColor();
            }
            
            if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
                
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
    }
};
```

## Menu Bar

```cpp
class MenuBar {
private:
    bool show_about = false;
    bool show_settings = false;
    
public:
    void render() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    newFile();
                }
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                    openFile();
                }
                if (ImGui::MenuItem("Save", "Ctrl+S", false, hasUnsavedChanges())) {
                    saveFile();
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Recent Files")) {
                    for (const auto& file : getRecentFiles()) {
                        if (ImGui::MenuItem(file.c_str())) {
                            openRecentFile(file);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    requestExit();
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo())) {
                    undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo())) {
                    redo();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Settings...", nullptr, &show_settings)) {
                    // Will show settings window
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Tool Window", nullptr, &show_tools);
                ImGui::MenuItem("Properties", nullptr, &show_properties);
                ImGui::MenuItem("Console", nullptr, &show_console);
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About", nullptr, &show_about)) {
                    // Will show about dialog
                }
                ImGui::EndMenu();
            }
            
            // Right-aligned status
            float width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width - 200);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            
            ImGui::EndMainMenuBar();
        }
        
        // Modals
        if (show_about) {
            ImGui::OpenPopup("About");
            show_about = false;
        }
        
        if (ImGui::BeginPopupModal("About", nullptr, 
            ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("My Application v1.0");
            ImGui::Text("Built with Dear ImGui %s", IMGUI_VERSION);
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
};
```

## Color Picker

```cpp
class ColorPicker {
private:
    ImVec4 color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    std::vector<ImVec4> palette = {
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f),  // Red
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f),  // Green  
        ImVec4(0.0f, 0.0f, 1.0f, 1.0f),  // Blue
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f),  // Yellow
        ImVec4(1.0f, 0.0f, 1.0f, 1.0f),  // Magenta
        ImVec4(0.0f, 1.0f, 1.0f, 1.0f),  // Cyan
    };
    
public:
    void render() {
        // Main color picker
        ImGui::ColorPicker4("##picker", (float*)&color,
            ImGuiColorEditFlags_NoSidePreview |
            ImGuiColorEditFlags_PickerHueBar |
            ImGuiColorEditFlags_AlphaBar);
            
        ImGui::SameLine();
        
        // Palette
        ImGui::BeginGroup();
        ImGui::Text("Palette");
        for (int i = 0; i < palette.size(); i++) {
            ImGui::PushID(i);
            if (ImGui::ColorButton("##palette", palette[i], 
                ImGuiColorEditFlags_NoTooltip, ImVec2(30, 30))) {
                color = palette[i];
            }
            if ((i + 1) % 3 != 0) ImGui::SameLine();
            ImGui::PopID();
        }
        
        // Add current to palette
        if (ImGui::Button("Add to Palette")) {
            palette.push_back(color);
        }
        
        ImGui::EndGroup();
        
        // Hex input
        float col[4] = { color.x, color.y, color.z, color.w };
        char hex[9];
        ImFormatString(hex, IM_ARRAYSIZE(hex), "%02X%02X%02X%02X",
            ImClamp((int)(col[0] * 255.0f), 0, 255),
            ImClamp((int)(col[1] * 255.0f), 0, 255),
            ImClamp((int)(col[2] * 255.0f), 0, 255),
            ImClamp((int)(col[3] * 255.0f), 0, 255));
            
        if (ImGui::InputText("Hex", hex, IM_ARRAYSIZE(hex),
            ImGuiInputTextFlags_CharsHexadecimal)) {
            // Parse hex
            unsigned int r, g, b, a;
            if (sscanf(hex, "%02X%02X%02X%02X", &r, &g, &b, &a) == 4) {
                color.x = r / 255.0f;
                color.y = g / 255.0f;
                color.z = b / 255.0f;
                color.w = a / 255.0f;
            }
        }
    }
};
```

## Toolbar

```cpp
class Toolbar {
private:
    struct Tool {
        std::string name;
        std::string icon;
        std::string tooltip;
        std::function<void()> action;
        bool enabled = true;
    };
    
    std::vector<Tool> tools;
    int selected_tool = -1;
    
public:
    void addTool(const std::string& name, const std::string& icon,
                 const std::string& tooltip, std::function<void()> action) {
        tools.push_back({name, icon, tooltip, action, true});
    }
    
    void render() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
        
        float button_size = 32.0f;
        
        for (int i = 0; i < tools.size(); i++) {
            const auto& tool = tools[i];
            
            if (!tool.enabled) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            }
            
            bool selected = (i == selected_tool);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            }
            
            if (ImGui::Button(tool.icon.c_str(), ImVec2(button_size, button_size))) {
                selected_tool = i;
                if (tool.action) tool.action();
            }
            
            if (selected) {
                ImGui::PopStyleColor();
            }
            
            if (ImGui::IsItemHovered() && !tool.tooltip.empty()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", tool.tooltip.c_str());
                ImGui::EndTooltip();
            }
            
            if (!tool.enabled) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            
            ImGui::SameLine();
        }
        
        ImGui::PopStyleVar(2);
    }
};
```

## Next Steps

- [Performance Tips →](performance.md) - Optimize your ImGui usage
- [Troubleshooting →](troubleshooting.md) - Common issues and solutions
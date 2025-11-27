#ifndef PDFJson_H
#define PDFJson_H

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include <iostream>
#include <string>
#include <vector>
#include "BriskyPdf.h"

using namespace rapidjson;

struct Color {
    double r = 0, g = 0, b = 0;
};
/*
struct TextObject {
    std::string content;
    std::string font_path="";
    double x = 0, y = 0;
    double font_size = 10;
    std::string v_alignment = "left";
    std::string h_alignment = "top";
    double max_width=0;
    double max_height=0;
    double line_space=10;
    Color color;
};

struct Cell {
    std::string content;
    int colspan = 1;
    int rowspan = 1;
    std::string h_alignment = "left";
    std::string v_alignment = "center";
    std::string font_path="";
    double font_size = 10;
    bool is_header = false;
    double border_width = 0.5;
    bool hidden = false;
};

struct Row {
    std::vector<Cell> cells;
    double height = 20;
    bool is_header = false;
    bool page_break_before = false;
};

struct Table {
    std::vector<Row> rows;
    double border_width = 1;
    double cell_padding = 4;
    double font_size = 9;
    std::string font_path;
    Color header_background;
    Color even_row_background;
    double table_width = 500;
    double start_x = 50;
    double start_y = 650;
};

struct Photo {
    std::string path;
    double x = 0, y = 0;
    double width = 0, height = 0;
    double scale = 0;
    double angle = 0;
    int index = 0;
};

struct Shape {
    std::string type;
    double x = 0, y = 0;
    double x2 = 0, y2 = 0;
    double x3 = 0, y3 = 0;
    double width = 0, height = 0;
    Color stroke_color;
    Color fill_color;
    double line_width = 0.5;
    double radius = 0.5;
};

struct PageObject {
    TextObject text;
    Table table;
    Photo photo;
    Shape shape;
    std::string type; // "text", "table", "photo", "shape"
};

struct Page {
    double margin = 20;
    std::vector<PageObject> objects;
};

struct HeaderFooter {
    std::vector<PageObject> objects;
};
*/
struct DocumentConfig {
    std::string file_name;
    double height = 842;
    double width = 595;
    double font_size = 10;
    std::string font_path;
    double margin = 5;
    double header_height = 120;
    double footer_height = 40;
};

class PDFJson {
private:
    std::shared_ptr<PDFCreator> pdf;
    DocumentConfig config;
    bool processSuccess = false;

    // Utility methods
    bool hasMember(const Value& obj, const char* name) const;
    std::string getString(const Value& obj, const char* name, const std::string& defaultValue = "") const;
    double getDouble(const Value& obj, const char* name, double defaultValue = 0.0) const;
    int getInt(const Value& obj, const char* name, int defaultValue = 0) const;
    bool getBool(const Value& obj, const char* name, bool defaultValue = false) const;

    // Parsing methods
    Color processColor(const Value& colorObj) const;
    bool processTextObject(PDFFormXObject *xObject,const Value& textObj, int pageNumber=0) const;
    std::shared_ptr<TableCell> processCell(PDFFormXObject *xObject,const Value& cellObj) const;
    std::shared_ptr<TableRow> processRow(PDFFormXObject *xObject,const Value& rowObj) const;
    bool processTable(PDFFormXObject *xObject,const Value& tableObj) const;
    bool processPhoto(PDFFormXObject *xObject,const Value& photoObj) const;
    bool processShape(PDFFormXObject *xObject,const Value& shapeObj) const;
    bool processPageObject(PDFFormXObject *xObject,const Value& obj, int pageNumber=0) const;
    bool processPage(const Value& pageObj) const;
    bool processHeaderFooter(PDFFormXObject *xObject,const Value& hfObj, int pageNumber=0) const;

   std::string replaceStr(const std::string &source, const std::string &from, const std::string &to) const;


public:
    PDFJson(std::shared_ptr<PDFCreator> pdf_);
    virtual ~PDFJson() = default;

    // Main parsing function
    bool processFromFile(const std::string& filename);
    bool processFromString(const std::string& jsonString);

    // Accessors
    const DocumentConfig& getConfig() const { return config; }
    bool isParsedSuccessfully() const { return processSuccess; }
    
    // Utility methods
    void clear();

    // Getters for specific elements
   // const std::vector<Page>& getPages() const { return config.pages; }
   // const HeaderFooter& getHeader() const { return config.header; }
   // const HeaderFooter& getFooter() const { return config.footer; }
    std::string getFileName() const { return config.file_name; }
    double getPageWidth() const { return config.width; }
    double getPageHeight() const { return config.height; }
};

#endif // PDFJson_H
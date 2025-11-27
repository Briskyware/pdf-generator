#include "BriskyPdfJson.h"
#include <fstream>
#include <sstream>

PDFJson::PDFJson(std::shared_ptr<PDFCreator> pdf_):pdf(pdf_) {
    clear();
}

void PDFJson::clear() {
    config = DocumentConfig();
    processSuccess = false;
}

bool PDFJson::hasMember(const Value& obj, const char* name) const {
    return obj.HasMember(name) && !obj[name].IsNull();
}

std::string PDFJson::getString(const Value& obj, const char* name, const std::string& defaultValue) const {
    return hasMember(obj, name) ? obj[name].GetString() : defaultValue;
}

double PDFJson::getDouble(const Value& obj, const char* name, double defaultValue) const {
    return hasMember(obj, name) ? obj[name].GetDouble() : defaultValue;
}

int PDFJson::getInt(const Value& obj, const char* name, int defaultValue) const {
    return hasMember(obj, name) ? obj[name].GetInt() : defaultValue;
}

bool PDFJson::getBool(const Value& obj, const char* name, bool defaultValue) const {
    return hasMember(obj, name) ? obj[name].GetBool() : defaultValue;
}

Color PDFJson::processColor(const Value& colorObj) const {
    Color color;
    if (colorObj.IsObject()) {
        color.r = getDouble(colorObj, "r");
        color.g = getDouble(colorObj, "g");
        color.b = getDouble(colorObj, "b");
    }
    return color;
}

bool PDFJson::processTextObject(PDFFormXObject *xObject,const Value& textObj,int pageNumber) const {
    if (textObj.IsObject()) {
        auto content = getString(textObj, "content");
        auto font_path = getString(textObj, "font_path");
        auto x = getDouble(textObj, "x");
        auto y = getDouble(textObj, "y");
        auto font_size = getDouble(textObj, "font_size", 10);
        auto v_alignment = getString(textObj, "v_alignment","top");
        auto h_alignment = getString(textObj, "h_alignment","left");
        auto max_height = getDouble(textObj, "max_height");
        auto max_width = getDouble(textObj, "max_width");
        auto line_space = getDouble(textObj, "line_space",10);
        Color color;

        if (hasMember(textObj, "color")) {
            color = processColor(textObj["color"]);
        }
        HAlignment hAlignment;
        VAlignment vAlignment;
        content = replaceStr(content ,"${PAGE_NUMBER}",std::to_string(pageNumber));

        if (h_alignment == "left")
            hAlignment = HAlignment::LEFT;
        else if (h_alignment == "right")
            hAlignment = HAlignment::RIGHT;
        else if (h_alignment == "center")
            hAlignment = HAlignment::CENTER;

        if (v_alignment == "top")
            vAlignment= VAlignment::TOP;
        else if (v_alignment == "bottom")
            vAlignment = VAlignment::BOTTOM;
        else if (v_alignment == "center")
            vAlignment = VAlignment::CENTER;

        auto font = pdf->getFont();
        if(font_path.length()>0)  font = pdf->getFontByPath(font_path);
        if(!content.empty())
        {
            pdf->addText(xObject,x,y,content,font,font_size,color.r,color.g,color.b,hAlignment,vAlignment,max_width,max_height,line_space,false);
        }
    }
    return true;
}

std::shared_ptr<TableCell> PDFJson::processCell(PDFFormXObject *xObject,const Value& cellObj) const {
    std::shared_ptr<TableCell> td = std::make_shared<TableCell>();
    if (cellObj.IsObject()) {
        td->content = getString(cellObj, "content");
        td->colspan = getInt(cellObj, "colspan", 1);
        td->rowspan = getInt(cellObj, "rowspan", 1);
        auto h_alignment = getString(cellObj, "h_alignment", "left");
        auto v_alignment = getString(cellObj, "v_alignment", "center");
        auto fontPath = getString(cellObj, "font_path");
        td->width = getDouble(cellObj, "width");
        td->fontSize = getDouble(cellObj, "font_size", 10);
        td->isHeader = getBool(cellObj, "is_header");
        td->borderWidth = getDouble(cellObj, "border_width", 0.5);
        td->topBorderWidth = getDouble(cellObj, "top_border_width", -1);
        td->leftBorderWidth = getDouble(cellObj, "left_border_width", -1);
        td->rightBorderWidth = getDouble(cellObj, "right_border_width", -1);
        td->bottomBorderWidth = getDouble(cellObj, "bottom_border_width", -1);
        if (hasMember(cellObj, "background_color")) {
              auto color = processColor(cellObj["background_color"]);
              td->backgroundColor.r=color.r;
              td->backgroundColor.g=color.g;
              td->backgroundColor.b=color.b;
              //std::cout<<"td->backgroundColor:"<< td->backgroundColor.r<<","<< td->backgroundColor.g<<","<< td->backgroundColor.b<<std::endl;
        }
        else
        {
             td->backgroundColor.r=1;
              td->backgroundColor.g=1;
              td->backgroundColor.b=1;
        }
        if (hasMember(cellObj, "text_color")) {
              auto color = processColor(cellObj["text_color"]);
              td->textColor.r=color.r;
              td->textColor.g=color.g;
              td->textColor.b=color.b;
        }
        else
        {
            td->textColor.r=0;
            td->textColor.g=0;
            td->textColor.b=0;
        }


        if (h_alignment == "left")
            td->hAlignment = HAlignment::LEFT;
        else if (h_alignment == "right")
            td->hAlignment = HAlignment::RIGHT;
        else if (h_alignment == "center")
            td->hAlignment = HAlignment::CENTER;

        if (v_alignment == "top")
            td->vAlignment= VAlignment::TOP;
        else if (v_alignment == "bottom")
            td->vAlignment = VAlignment::BOTTOM;
        else if (v_alignment == "center")
            td->vAlignment = VAlignment::CENTER;

        td->font = pdf->getFont();
        if(fontPath.length()>0)  td->font = pdf->getFontByPath(fontPath);
    }
    return td;
}

std::shared_ptr<TableRow> PDFJson::processRow(PDFFormXObject *xObject,const Value& rowObj) const {
    std::shared_ptr<TableRow> tr = std::make_shared<TableRow>();
    if (rowObj.IsObject()) {
        tr->height = getDouble(rowObj, "height", 20);
        tr->isHeader = getBool(rowObj, "is_header");
        tr->pageBreakBefore = getBool(rowObj, "page_break_before");
        
        if (hasMember(rowObj, "cells") && rowObj["cells"].IsArray()) {
            const Value& cellsArray = rowObj["cells"];
            for (SizeType i = 0; i < cellsArray.Size(); i++) {
                tr->cells.push_back(processCell(xObject,cellsArray[i]));
            }
        }
    }
    return tr;
}

bool PDFJson::processTable(PDFFormXObject *xObject,const Value& tableObj) const {
    if (tableObj.IsObject()) {
        auto tableDrawer = pdf->CreateTable();
        TableStyle style;

        style.borderWidth = getDouble(tableObj, "border_width", 0.5);
        style.cellPadding = getDouble(tableObj, "cell_padding", 2.0);
        style.fontSize = getDouble(tableObj, "font_size", config.font_size);
        auto fontPath = getString(tableObj, "font_path");
        auto tableWidth = getDouble(tableObj, "table_width", 500);
        auto startX = getDouble(tableObj, "start_x", 50);
        auto startY = getDouble(tableObj, "start_y", 650);

        if (hasMember(tableObj, "header_background")) {
              auto color = processColor(tableObj["header_background"]);
              style.headerBackground.r=color.r;
              style.headerBackground.g=color.g;
              style.headerBackground.b=color.b;
        }
        else
        {
            style.headerBackground.r=0.9;
            style.headerBackground.g=0.9;
            style.headerBackground.b=0.9;
        }
        
        if (hasMember(tableObj, "even_row_background")) {
            auto color = processColor(tableObj["even_row_background"]);
            style.evenRowBackground.r=color.r;
            style.evenRowBackground.g=color.g;
            style.evenRowBackground.b=color.b;
        }
        else
        {
            style.evenRowBackground.r=0.97;
            style.evenRowBackground.g=0.97;
            style.evenRowBackground.b=0.97;
        }

        if (hasMember(tableObj, "odd_row_background")) {
            auto color = processColor(tableObj["odd_row_background"]);
            style.oddRowBackground.r=color.r;
            style.oddRowBackground.g=color.g;
            style.oddRowBackground.b=color.b;
        }
        else
        {
            style.oddRowBackground.r=1.0;
            style.oddRowBackground.g=1.0;
            style.oddRowBackground.b=1.0;
        }

        if (hasMember(tableObj, "border_color")) {
            auto color = processColor(tableObj["border_color"]);
            style.borderColor.r=color.r;
            style.borderColor.g=color.g;
            style.borderColor.b=color.b;
        }
        else
        {
            style.borderColor.r=0.5;
            style.borderColor.g=0.5;
            style.borderColor.b=0.5;
        }

        if (hasMember(tableObj, "text_color")) {
            auto color = processColor(tableObj["text_color"]);
            style.textColor.r=color.r;
            style.textColor.g=color.g;
            style.textColor.b=color.b;
        }
        else
        {
            style.textColor.r=0;
            style.textColor.g=0;
            style.textColor.b=0;
        }
        
        style.font = pdf->getFont();
        if(fontPath.length()>0)  style.font = pdf->getFontByPath(fontPath);

        tableDrawer->SetStyle(style);

        std::vector<TableRow> rows;
        if (hasMember(tableObj, "rows") && tableObj["rows"].IsArray()) {
            const Value& rowsArray = tableObj["rows"];
            for (SizeType i = 0; i < rowsArray.Size(); i++) {
                rows.push_back(*(processRow(xObject,rowsArray[i])));
            }
        }
       
        pdf->DrawTableWithPageBreaks(xObject,tableDrawer, rows, startX, startY, tableWidth);
    }
   
    return true;
}

bool PDFJson::processPhoto(PDFFormXObject *xObject,const Value& photoObj) const {
    //Photo photo;
    if (photoObj.IsObject()) {
        auto path = getString(photoObj, "path");
        auto x = getDouble(photoObj, "x");
        auto y = getDouble(photoObj, "y");
        auto width = getDouble(photoObj, "width");
        auto height = getDouble(photoObj, "height");
        auto scale = getDouble(photoObj, "scale");
        auto angle = getDouble(photoObj, "angle");
        auto index = getInt(photoObj, "index");
        pdf->embedImage(xObject,path, x, y, width, height, scale, angle, index);
        return true;
    }
    
    return false;
}

bool PDFJson::processShape(PDFFormXObject *xObject,const Value& shapeObj) const {
    if (shapeObj.IsObject()) {
        auto type = getString(shapeObj, "type");
        auto x = getDouble(shapeObj, "x");
        auto y = getDouble(shapeObj, "y");
        auto x2 = getDouble(shapeObj, "x2");
        auto y2 = getDouble(shapeObj, "y2");
        auto x3 = getDouble(shapeObj, "x3");
        auto y3 = getDouble(shapeObj, "y3");
        auto width = getDouble(shapeObj, "width");
        auto height = getDouble(shapeObj, "height");
        auto radius = getDouble(shapeObj, "radius");
        auto line_width = getDouble(shapeObj, "line_width", 0.5);
        Color stroke_color;
        Color fill_color;
        
        if (hasMember(shapeObj, "stroke_color")) {
            stroke_color = processColor(shapeObj["stroke_color"]);
        }
        else
        {
            stroke_color.r=0;
            stroke_color.g=0;
            stroke_color.b=0;

        }
        if (hasMember(shapeObj, "fill_color")) {
            fill_color = processColor(shapeObj["fill_color"]);
        }
        else
        {
            fill_color.r=-1;
            fill_color.g=-1;
            fill_color.b=-1;
        }

        if (type == "circle" && radius>0)
        {
            pdf->addCircle(xObject,x, y, radius,
                            fill_color.r, fill_color.g, fill_color.b,
                            stroke_color.r, stroke_color.g, stroke_color.b,
                            line_width);
        }
        else if ((type == "rectangle" || type == "square") &&
                width>0 && height>0)
        {
            pdf->addRectangle(xObject,x, y, width , height,
                             fill_color.r,  fill_color.g,  fill_color.b,
                             stroke_color.r,  stroke_color.g,  stroke_color.b,
                             line_width);
        }
        else if ( type == "line" &&  x2>0 &&  y2>0)
        {
            pdf->addLine(xObject, x,  y,  x2,  y2,
                         line_width,  stroke_color.r,  stroke_color.g,  stroke_color.b);
        }
        else if ( type == "triangle" &&  x2>0 &&  y2>0 &&
                 x3>0 &&  y3>0)
        {
            pdf->addTriangle(xObject, x,  y,  x2,  y2,  x3,  y3,
                             fill_color.r,  fill_color.g,  fill_color.b,
                             stroke_color.r,  stroke_color.g,  stroke_color.b,
                             line_width);
        }
        else
        {
            return false;
        }
    }
    return true;
}

bool PDFJson::processPageObject(PDFFormXObject *xObject,const Value& obj, int pageNumber) const {

    auto type = getString(obj, "type");
    if (type=="photo") {
        if(!processPhoto(xObject,obj))
        {
            std::cout<<"Error: Failed to process photo at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    } else if (type=="text")
    {
        if(!processTextObject(xObject,obj,pageNumber))
        {
            std::cout<<"Error: Failed to process Text at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    } else if (type=="table")
    {
        if(!processTable(xObject,obj))
        {
            std::cout<<"Error: Failed to process Table at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    } else if (type=="line" || type=="circle" || type == "rectangle" || type == "square" || type == "triangle")
    {
        if(!processShape(xObject,obj))
        {
            std::cout<<"Error: Failed to process Shape at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    }

    /*if (hasMember(obj, "photo")) {
        if(!processPhoto(xObject,obj["photo"]))
        {
            std::cout<<"Error: Failed to process photo at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    }
    if (hasMember(obj, "text")) {
        if(!processTextObject(xObject,obj["text"],pageNumber))
        {
            std::cout<<"Error: Failed to process Text at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    }
    if (hasMember(obj, "table")) {
        if(!processTable(xObject,obj["table"]))
        {
            std::cout<<"Error: Failed to process Table at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    }
    if (hasMember(obj, "shape")) {
        if(!processShape(xObject,obj["shape"]))
        {
            std::cout<<"Error: Failed to process Shape at page number" + std::to_string(pageNumber)<<std::endl;
            return false;
        }
    }*/
    
    
    return true;
}

bool PDFJson::processPage(const Value& pageObj) const {
    if (pageObj.IsObject()) {
        auto footerHeight = getDouble(pageObj, "footer_height");
        auto headerHeight = getDouble(pageObj, "header_height");
        auto margin = getDouble(pageObj, "margin");
        if(footerHeight==0) footerHeight = config.footer_height;
        if(headerHeight==0) headerHeight = config.header_height;
        if(margin==0) margin = config.margin;
        pdf->setPageStyle(margin, headerHeight, footerHeight);
        pdf->createNewPage();
        
        if (hasMember(pageObj, "objects") && pageObj["objects"].IsArray()) {
            const Value& objectsArray = pageObj["objects"];
            for (SizeType i = 0; i < objectsArray.Size(); i++) {
                processPageObject(nullptr,objectsArray[i]);
            }
        }
    }
    return true;
}

bool PDFJson::processHeaderFooter(PDFFormXObject *xObject,const Value& hfObj, int pageNumber) const {

    if (hfObj.IsObject() && hasMember(hfObj, "objects") && hfObj["objects"].IsArray()) {
        const Value& objectsArray = hfObj["objects"];
        for (SizeType i = 0; i < objectsArray.Size(); i++) {
            if(!processPageObject(xObject,objectsArray[i],pageNumber))
            {
                std::cout<<"Error: Failed to process Object " + std::to_string(i) +
                      " on footer_header " + std::to_string(pageNumber)<<std::endl;
                return false;
            }
        }
    }
    return true;
}

bool PDFJson::processFromFile(const std::string& filename) {
    clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    
    
    return processFromString(buffer.str());
}

bool PDFJson::processFromString(const std::string& jsonString) {
    clear();
    
    Document document;
    document.Parse(jsonString.c_str());
    
    if (document.HasParseError()) {
        std::cerr << "Error: JSON process error" << std::endl;
        return false;
    }
    
    if (!document.IsObject()) {
        std::cerr << "Error: Root is not an object" << std::endl;
        return false;
    }
    
    // Parse main document configuration (same as processFromFile)
    config.file_name = getString(document, "file_name");
    config.height = getDouble(document, "height", 842);
    config.width = getDouble(document, "width", 595);
    config.font_size = getDouble(document, "font_size", 10);
    config.font_path = getString(document, "font_path");
    config.margin = getDouble(document, "margin", 5);
    config.header_height = getDouble(document, "header_height", 120);
    config.footer_height = getDouble(document, "footer_height", 40);

    pdf = std::make_shared<PDFCreator>(config.width , config.height, config.margin, config.header_height, config.footer_height);

    if (!pdf->createDocument(config.file_name))
    {
        std::cerr << "Error: Failed to create pdf file" << std::endl;
        return false;
    }
    if (hasMember(document, "header")|| hasMember(document, "footer"))
      {
        pdf->initPageFunc = [&document, this](int p)
        {
          ObjectIDType headerId;
          ObjectIDType footerId;
          if (hasMember(document, "header"))
          {
            auto header = pdf->createHeader();
            if (!processHeaderFooter(header,document["header"],p))
            {
                std::cerr << "Warning: Failed to process header" << std::endl;
            }
            headerId = pdf->closeXObject(header);
          }
          if (hasMember(document, "footer"))
          {
            auto footer = pdf->createFooter();
            if (!processHeaderFooter(footer,document["footer"],p))
            {
                std::cerr << "Warning: Failed to process footer" << std::endl;
            }
            footerId = pdf->closeXObject(footer);
          }

          if (hasMember(document, "header"))
          {
            pdf->addHeader(headerId);
          }
          if (hasMember(document, "footer"))
          {
            pdf->addFooter(footerId);
          }
        };
      }

        
    // Parse pages array
    if (hasMember(document, "pages") && document["pages"].IsArray()) {
        const Value& pagesArray = document["pages"];
        for (SizeType i = 0; i < pagesArray.Size(); i++) {
            if(!processPage(pagesArray[i]))
            {
                std::cout<<"Error: Failed to process page " + std::to_string(i)<<std::endl;
                return false;
            }
        }
    }

    if (pdf->saveDocument()) {
        std::cout<<"PDF report generated successfully!"<<std::endl;
    } else {
        std::cout<<"ERROR:Failed to generate PDF report!!"<<std::endl;
        return false;
    }
    
    processSuccess = true;
    return true;
}

std::string PDFJson::replaceStr(const std::string &source, const std::string &from, const std::string &to) const
{
    std::string str(source);
    if (from.empty())
        return str;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

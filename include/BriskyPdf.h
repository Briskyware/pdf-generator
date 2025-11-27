#ifndef PDFCREATOR_H
#define PDFCREATOR_H

#include <string>
#include <vector>
#include <map>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <iostream>
#include "PDFWriter/PDFWriter.h"
#include "PDFWriter/PDFPage.h"
#include "PDFWriter/PageContentContext.h"
#include "PDFWriter/AbstractContentContext.h"
#include "PDFWriter/PDFImageXObject.h"
#include "PDFWriter/XObjectContentContext.h"
#include "PDFWriter/PDFFormXObject.h"
#include "PDFWriter/PDFUsedFont.h"



using namespace PDFHummus;

class PDFCreator;

struct Dimension {
    double x=0;
    double y=0;
    double width=0;
    double height=0;
    bool ok=false;
};

enum class HAlignment { LEFT, CENTER, RIGHT };
enum class VAlignment { TOP, CENTER, BOTTOM };

struct TableStyle {
    double borderWidth = 0.5;
    double cellPadding = 4.0;
    double fontSize = 10.0;
    std::shared_ptr<PDFUsedFont> font;
    
    // Colors (RGB)
    struct Color {
        double r, g, b;
        Color(double red = -1, double green = -1, double blue = -1) : r(red), g(green), b(blue) {}
    };
    
    Color borderColor = Color(0.5, 0.5, 0.5);
    Color headerBackground = Color(0.9, 0.9, 0.9);
    Color evenRowBackground = Color(0.97, 0.97, 0.97);
    Color oddRowBackground = Color(1.0, 1.0, 1.0);
    Color textColor = Color(0, 0, 0);
    
    // Alignment
    

};

struct TableCell {
    std::string content;
    int colspan = 1;
    int rowspan = 1;
    HAlignment hAlignment = HAlignment::LEFT;
    VAlignment vAlignment =VAlignment::CENTER;
    TableStyle::Color backgroundColor;
    TableStyle::Color textColor;
    double fontSize = -1; // -1 means use default
    double width=0;
    double borderWidth=-1;
    bool isHeader = false;
    std::shared_ptr<PDFUsedFont> font=nullptr;
    
    // For internal tracking
    int actualRow = -1;
    int actualCol = -1;
    bool isSpanned = false;
    double height=0;
    bool isRowHeader = false;
    double topBorderWidth=-1;
    double leftBorderWidth=-1;
    double rightBorderWidth=-1;
    double bottomBorderWidth=-1;
    
   
};

struct TableRow {
    std::vector<std::shared_ptr<TableCell>> cells;
    double height = 20.0; // Default row height
    bool isHeader = false;
    bool pageBreakBefore = false;
};

struct CellPosition {
        int row, col;
        double x, y, width, height;
        std::shared_ptr<TableCell> cell;
    };


class AdvancedTextWrapper {
private:
    std::shared_ptr<PDFUsedFont> mFont;
    double mFontSize;
    double mLineHeight;
    double mLineSpace;
    PDFUsedFont::TextMeasures mTextMeasure;

public:
    AdvancedTextWrapper(std::shared_ptr<PDFUsedFont> font, double fontSize,double lineSpace);
       

    struct WrappingOptions {
        double maxWidth;
        bool hyphenate = false;
        HAlignment alignment = HAlignment::LEFT;
    };

    

    struct WrappedTextResult {
        double totalHeight;
        std::vector<std::string> lines;
        std::vector<double> lineWidths;
        double lineHeight;
        double lineSpace;
    };

    WrappedTextResult wrapText(const std::string &text, const WrappingOptions &options);

private:
    std::vector<std::string> splitWords(const std::string& text);
    
    std::pair<std::string, std::string> breakWord(const std::string& word, double maxWidth);
       
};


class PDFTable{
    private:
        PDFCreator* pdf=nullptr; 
        TableStyle mStyle;
        double mCurrentY;
        double margin;
       // int mCurrentPage;
    public:
    PDFTable(PDFCreator* pdf_ =nullptr,double pageMargin=50):pdf(pdf_),margin(pageMargin){}
     void SetStyle(const TableStyle& style) { mStyle = style; }
     friend class PDFCreator;
    
    
    
private:
    std::vector<std::vector<std::shared_ptr<TableCell>>> BuildCellGrid(const std::vector<TableRow>& rows);
    
    std::vector<CellPosition> CalculateCellPositions(
        const std::vector<std::vector<std::shared_ptr<TableCell>>>& grid,
        double tableWidth);
    
    
    int DrawRowsOnPage(PDFFormXObject *FormXObject,PageContentContext* context,
                      const std::vector<TableRow>& rows,
                      const std::vector<CellPosition>& cellPositions,
                      double startX, double startY, double tableWidth,
                      int startRow);
    
    void DrawRowCells(PDFFormXObject *FormXObject,PageContentContext* context,
                     const std::vector<CellPosition>& cellPositions,
                     int rowIdx, double startX, double rowY, double tableWidth);

    void DrawCell(PDFFormXObject *FormXObject,PageContentContext* context, const TableCell& cell,
                  double x, double y, double width, double height);
    
    void DrawCellBackground(PDFFormXObject *FormXObject,PageContentContext* context, const TableCell& cell,
                           double x, double y, double width, double height);
    
    void DrawCellBorder(PDFFormXObject *FormXObject,PageContentContext* context,
                       double x, double y, double width, double height,double borderWidth,
                     double topBorderWidth=-1,double bottomBorderWidth=-1,double leftBorderWidth=-1,double rightBorderWidth=-1);
    
    void DrawCellContent(PDFFormXObject *FormXObject,PageContentContext* context, const TableCell& cell,
                        double x, double y, double width, double height);
    
    
    double GetHeaderHeight(const std::vector<TableRow>& rows);
    
    void DrawTableHeader(PDFFormXObject *FormXObject,PageContentContext* context,
                        const std::vector<TableRow>& rows,
                        const std::vector<CellPosition>& cellPositions,
                        double startX, double startY, double tableWidth);


    
};


class PDFCreator {
protected:
    PDFWriter pdfWriter;
    std::string currentFilename;
    PDFPage* currentPage;
    PageContentContext* currentContext;
    std::shared_ptr<PDFUsedFont> font;
    double pageWidth;
    double pageHeight;
    std::map<std::string, PDFImageXObject*> imageCache;
    std::map<std::string, std::shared_ptr<PDFUsedFont>> fontCache;
    std::vector<std::shared_ptr<PDFTable>> tables;
    
 
    struct {
        double headerHeight;
        double footerHeight;
        double margin;

    } pageStyle;

public:
    int pageNumber;
    PDFCreator(double width=595,double height=842,double margin=50, double headerHeight=60,double footerHeight=40);
    ~PDFCreator();

    std::function<void(int)> initPageFunc;
    
    // Document operations
    bool createDocument(const std::string& filename);
    bool openDocument(const std::string& filename);
    bool saveDocument();
    void closeDocument();
    
    // Page operations
    void createNewPage();
    void setPageStyle(double margin = 50, double headerHeight = 60, double footerHeight = 40);
    
    // Drawing operations
    PDFFormXObject* createHeader();
    PDFFormXObject* createFooter();
    ObjectIDType closeXObject(PDFFormXObject* formXObject);
    void addHeader(ObjectIDType FormXObjectId);
    void addFooter(ObjectIDType FormXObjectId);
    
     Dimension addText(PDFFormXObject *FormXObject,double x, double y, const std::string& text, std::shared_ptr<PDFUsedFont> textFont,double fontSize = 12, 
                 double r = 0, double g = 0, double b = 0,HAlignment hAlignment=HAlignment::LEFT,
                 VAlignment vAlignment=VAlignment::TOP,
                 double maxWidth=0,double maxheight=0,double lineSpace=10,bool isHidden=false);
    

    Dimension addHorizontalLine(PDFFormXObject *FormXObject,double y, double lineWidth = 1);
    Dimension addVerticalLine(PDFFormXObject *FormXObject,double x, double lineWidth = 1);
    Dimension addLine(PDFFormXObject *FormXObject,double startX, double startY, double endX, double endY, 
                 double lineWidth = 1, double r = 0, double g = 0, double b = 0);

    void setFont(const std::string& fontPath="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        
    std::shared_ptr<PDFUsedFont> getFont()  {return font;};
    std::shared_ptr<PDFUsedFont> getFontByPath(const std::string& fontPath);
    
    // Shapes
    Dimension addRectangle(PDFFormXObject *FormXObject,double x, double y, double width, double height, 
                      double fillR = -1, double fillG = -1, double fillB = -1,
                      double strokeR = 0, double strokeG = 0, double strokeB = 0,
                      double lineWidth = 1);
    Dimension addCircle(PDFFormXObject *FormXObject,double centerX, double centerY, double radius,
                   double fillR = -1, double fillG = -1, double fillB = -1,
                   double strokeR = 0, double strokeG = 0, double strokeB = 0,
                   double lineWidth = 1);
    Dimension addTriangle(PDFFormXObject *FormXObject,double x1, double y1, double x2, double y2, double x3, double y3,
                     double fillR = -1, double fillG = -1, double fillB = -1,
                     double strokeR = 0, double strokeG = 0, double strokeB = 0,
                     double lineWidth = 1);
    
       
    
    // Utility functions
    double getPageWidth() const { return pageWidth; }
    double getPageHeight() const { return pageHeight; }
    double getContentHeight() const { return pageHeight - pageStyle.headerHeight - pageStyle.footerHeight - (2 * pageStyle.margin); }

    // Image operations
    Dimension embedImage(PDFFormXObject *FormXObject,const std::string& imagePath,double x, double y, double width=0, double height=0, double scale=0, double angle=0,int index=0) ;
    
    // Utility functions
    void getImageDimensions(const std::string& imagePath, double& width, double& height);
    void clearImageCache();

    //Table functions
    std::shared_ptr<PDFTable> CreateTable();
    Dimension DrawTableWithPageBreaks(PDFFormXObject *FormXObject,std::shared_ptr<PDFTable> table,const std::vector<TableRow>& rows, 
                                double startX, double startY,
                                double tableWidth); 
    

protected:
    void drawCircle(PDFFormXObject *FormXObject,PageContentContext* context, double centerX, double centerY, double radius);
    PDFImageXObject* getImageXObject(const std::string& imagePath);

    Dimension DrawTableOnPages(PDFFormXObject *FormXObject,std::shared_ptr<PDFTable> table,
                         const std::vector<TableRow>& rows,
                         const std::vector<CellPosition>& cellPositions,
                         double startX, double tableWidth);

};

#endif

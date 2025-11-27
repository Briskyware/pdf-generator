#include "BriskyPdf.h"
#include <iostream>
#include <cmath>

AdvancedTextWrapper::AdvancedTextWrapper(std::shared_ptr<PDFUsedFont> font, double fontSize, double lineSpace)
    : mFont(font), mFontSize(fontSize), mLineSpace(lineSpace)
{
    auto dimensions = mFont->CalculateTextDimensions("X", mFontSize);
    mLineHeight = dimensions.height;
}

AdvancedTextWrapper::WrappedTextResult AdvancedTextWrapper::wrapText(const std::string &text, const WrappingOptions &options)
{
    WrappedTextResult result;
    result.totalHeight = 0;
    result.lineHeight = mLineHeight;
    result.lineSpace = mLineSpace;

    std::vector<std::string> words = splitWords(text);
    std::string currentLine;
    double currentLineWidth = 0;

    for (size_t i = 0; i < words.size(); ++i)
    {
        const std::string &word = words[i];
        auto dimensions = mFont->CalculateTextDimensions(word, mFontSize);
        auto spaceDimensions = mFont->CalculateTextDimensions(" ", mFontSize);
        double wordWidth = dimensions.width;
        double spaceWidth = currentLine.empty() ? 0 : spaceDimensions.width;

        if (currentLineWidth + spaceWidth + wordWidth <= options.maxWidth)
        {
            if (!currentLine.empty())
            {
                currentLine += " ";
                currentLineWidth += spaceWidth;
            }
            currentLine += word;
            currentLineWidth += wordWidth;
        }
        else
        {
            if (currentLine.empty())
            {
                if (options.hyphenate)
                {
                    auto brokenWord = breakWord(word, options.maxWidth);
                    currentLine = brokenWord.first;
                    auto dimensions = mFont->CalculateTextDimensions(currentLine + "-", mFontSize);
                    result.lines.push_back(currentLine + "-");
                    result.lineWidths.push_back(dimensions.width);
                    result.totalHeight += mLineHeight + mLineSpace;

                    words.insert(words.begin() + i + 1, brokenWord.second);
                }
                else
                {
                    result.lines.push_back(word);
                    result.lineWidths.push_back(wordWidth);
                    result.totalHeight += mLineHeight + mLineSpace;
                }
                currentLine.clear();
                currentLineWidth = 0;
            }
            else
            {
                result.lines.push_back(currentLine);
                result.lineWidths.push_back(currentLineWidth);
                result.totalHeight += mLineHeight + mLineSpace;

                currentLine = word;
                currentLineWidth = wordWidth;
            }
        }
    }

    if (!currentLine.empty())
    {
        result.lines.push_back(currentLine);
        result.lineWidths.push_back(currentLineWidth);
        result.totalHeight += mLineHeight;
    }

    return result;
}

std::vector<std::string> AdvancedTextWrapper::splitWords(const std::string &text)
{
    std::vector<std::string> words;
    std::istringstream stream(text);
    std::string word;

    while (stream >> word)
    {
        words.push_back(word);
    }

    return words;
}

std::pair<std::string, std::string> AdvancedTextWrapper::breakWord(const std::string &word, double maxWidth)
{
    // Simple hyphenation - break at approximately 80% of max width
    double targetWidth = maxWidth * 0.8;
    size_t breakPoint = word.length() / 2; // Simple center break

    for (size_t i = breakPoint; i > 0; --i)
    {
        std::string part1 = word.substr(0, i);
        auto dimensions = mFont->CalculateTextDimensions(part1, mFontSize);
        if (dimensions.width <= targetWidth)
        {
            return {part1, word.substr(i)};
        }
    }

    // If no good break point found, break at first possible position
    return {word.substr(0, 1), word.substr(1)};
}

////////////////////////////////////////////////////////////

std::vector<std::vector<std::shared_ptr<TableCell>>> PDFTable::BuildCellGrid(const std::vector<TableRow> &rows)
{
    int maxCols = 0;
    for (const auto &row : rows)
    {
        int colCount = 0;
        for (const auto &cell : row.cells)
        {
            colCount += cell->colspan;
        }
        maxCols = std::max(maxCols, colCount);
    }

    std::vector<std::vector<std::shared_ptr<TableCell>>> grid(rows.size(),
                                                              std::vector<std::shared_ptr<TableCell>>(maxCols, nullptr));

    for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx)
    {
        const auto &row = rows[rowIdx];
        int colIdx = 0;

        for (const auto &cell : row.cells)
        {
            while (colIdx < maxCols && grid[rowIdx][colIdx] != nullptr)
            {
                colIdx++;
            }

            if (colIdx >= maxCols)
                break;

            for (int r = 0; r < cell->rowspan && (rowIdx + r) < rows.size(); ++r)
            {
                for (int c = 0; c < cell->colspan && (colIdx + c) < maxCols; ++c)
                {
                    if (r == 0 && c == 0)
                    {
                        cell->height = row.height;
                        cell->isRowHeader = row.isHeader;
                        grid[rowIdx + r][colIdx + c] = cell;
                    }
                    else
                    {
                        std::shared_ptr<TableCell> spannedCell = std::make_shared<TableCell>();
                        spannedCell->isSpanned = true;
                        spannedCell->actualRow = rowIdx + r;
                        spannedCell->actualCol = colIdx + c;
                        spannedCell->height = rows[rowIdx + r].height;
                        spannedCell->width = grid[rowIdx][colIdx]->width;
                        spannedCell->isRowHeader = grid[rowIdx][colIdx]->isHeader;
                        grid[rowIdx + r][colIdx + c] = spannedCell;
                    }
                }
            }

            colIdx += cell->colspan;
        }
    }

    return grid;
}

// Calculate precise cell positions and sizes
std::vector<CellPosition> PDFTable::CalculateCellPositions(
    const std::vector<std::vector<std::shared_ptr<TableCell>>> &grid,
    double tableWidth)
{

    std::vector<CellPosition> positions;

    if (grid.empty())
        return positions;

    int numCols = grid[0].size();
    int numRows = grid.size();

    // Calculate column widths (simple equal distribution for now)
    double fixWidth = tableWidth / numCols;
    std::vector<double> colWidths(numCols, fixWidth);

    // Calculate row heights
    std::vector<double> rowHeights(numRows, mStyle.fontSize + (2 * mStyle.cellPadding));


    for (int row = 0; row < numRows; ++row)
    {
        for (int col = 0; col < numCols;)
        {
            if(grid[row][col]==nullptr) {col++; continue;}
            std::shared_ptr<TableCell> cell = grid[row][col];
            if(cell->width > 0 ) 
            {
                colWidths[col] = cell->width;
            }
            col+=cell->colspan;
        }
    }

    double sumWidths=0;
    double fixWidthCount=0;
    for(auto &item:colWidths) {
        sumWidths+=item;
        if(item==fixWidth) fixWidthCount++;
    }
    auto diff = (tableWidth-sumWidths)/fixWidthCount;
    for(auto &item:colWidths) {
        if(item==fixWidth) item+=diff;
    }

    for (int row = 0; row < numRows; ++row)
    {
        for (int col = 0; col < numCols; ++col)
        {
            if(grid[row][col]==nullptr) {continue;}
            grid[row][col]->width = colWidths[col];
        }
    }

    for (int row = 0; row < numRows; ++row)
    {
        double maxRowHeight = 0;
        for (int col = 0; col < numCols; ++col)
        {
            if(grid[row][col]==nullptr) {continue;}
            std::shared_ptr<TableCell> cell = grid[row][col];
            double fontSize = cell->fontSize > 0 ? cell->fontSize : mStyle.fontSize;
            std::shared_ptr<PDFUsedFont> fontID;
            if (!cell->font)
            {
                fontID = mStyle.font;
            }
            else
            {
                fontID = cell->font;
            }
            AdvancedTextWrapper wrapper(fontID, fontSize, 10);
            AdvancedTextWrapper::WrappingOptions options;
            double textWith=0;
            for (int c = col; c < col + cell->colspan && c < numCols; ++c)
            {
                textWith += colWidths[c];
            }

            options.maxWidth = textWith-(2*mStyle.cellPadding);
            options.alignment = HAlignment::LEFT;
            options.hyphenate = true;
            auto dimension = wrapper.wrapText(cell->content, options);
           double totalHeight =  dimension.totalHeight+(mStyle.cellPadding*2);
           if(totalHeight>cell->height)
            {
                cell->height = totalHeight;
            }
            if(maxRowHeight<cell->height) 
            {
                maxRowHeight = cell->height;
                for (int col = 0; col < numCols; ++col)
                {
                    if(grid[row][col]==nullptr) {continue;}
                    grid[row][col]->height = maxRowHeight;
                }
            }

        }
    }

    // Build cell positions
    for (int row = 0; row < numRows; ++row)
    {
        for (int col = 0; col < numCols; ++col)
        {
            if(grid[row][col]==nullptr) {continue;}
            std::shared_ptr<TableCell> cell = grid[row][col];
            if (cell)
            {
                CellPosition pos;
                pos.row = row;
                pos.col = col;

                pos.width = 0;

                for (int c = col; c < col + cell->colspan && c < numCols; ++c)
                {
                    //cell->width = colWidths[c];
                    pos.width += colWidths[c];
                }

                pos.height = 0;
                for (int r = row; r < row + cell->rowspan && r < numRows; ++r)
                {
                    if (cell->height > 0)
                        pos.height += cell->height;
                    else
                        pos.height += rowHeights[r];
                }

                pos.cell = cell;
                positions.push_back(pos);
            }
        }
    }

    return positions;
}

int PDFTable::DrawRowsOnPage(PDFFormXObject *FormXObject, PageContentContext *context,
                             const std::vector<TableRow> &rows,
                             const std::vector<CellPosition> &cellPositions,
                             double startX, double startY, double tableWidth,
                             int startRow)
{

    double currentY = startY;
    int rowsDrawn = 0;

    for (int rowIdx = startRow; rowIdx < static_cast<int>(rows.size()); ++rowIdx)
    {
        const auto &row = rows[rowIdx];
        double rowHeight = row.cells[0]->height;

        if (row.pageBreakBefore && rowsDrawn > 0)
        {
            return rowsDrawn; // Force page break
        }

        if (currentY - rowHeight < margin)
        {
            return rowsDrawn; // No more space
        }

        DrawRowCells(FormXObject, context, cellPositions, rowIdx, startX, currentY, tableWidth);

        currentY -= rowHeight;
        rowsDrawn++;
    }

    return rowsDrawn;
}

void PDFTable::DrawRowCells(PDFFormXObject *FormXObject, PageContentContext *context,
                            const std::vector<CellPosition> &cellPositions,
                            int rowIdx, double startX, double rowY, double tableWidth)
{
    double sx = startX;
    for (const auto &pos : cellPositions)
    {
        if (pos.row == rowIdx && pos.cell->isSpanned)
        {
            sx += pos.cell->width;
        }
        else if (pos.row == rowIdx && !pos.cell->isSpanned)
        {
            DrawCell(FormXObject, context, *pos.cell, sx, rowY, pos.width, pos.height);
            sx += pos.cell->width;
        }
    }
}

void PDFTable::DrawCell(PDFFormXObject *FormXObject, PageContentContext *context, const TableCell &cell,
                        double x, double y, double width, double height)
{
    XObjectContentContext *xobjectContentContext;
    if (FormXObject != nullptr)
        xobjectContentContext = FormXObject->GetContentContext();

    if (FormXObject == nullptr)
        context->q();
    else
        xobjectContentContext->q();
    DrawCellBackground(FormXObject, context, cell, x, y, width, height);
    DrawCellBorder(FormXObject, context, x, y, width, height, cell.borderWidth >= 0 ? cell.borderWidth : mStyle.borderWidth,cell.topBorderWidth,cell.bottomBorderWidth,cell.leftBorderWidth,cell.rightBorderWidth);
    DrawCellContent(FormXObject, context, cell, x, y, width, height);
    if (FormXObject == nullptr)
        context->Q();
    else
        xobjectContentContext->Q();
}

void PDFTable::DrawCellBackground(PDFFormXObject *FormXObject, PageContentContext *context, const TableCell &cell,
                                  double x, double y, double width, double height)
{

    TableStyle::Color bgColor = mStyle.oddRowBackground;

    if (cell.backgroundColor.r >= 0)
    {
        bgColor = cell.backgroundColor;
    }
    else if (cell.isHeader)
    {
        bgColor = mStyle.headerBackground;
    }
    if (bgColor.r == -1 || bgColor.g == -1 || bgColor.b == -1)
        return;

    if (FormXObject == nullptr)
    {
        context->rg(bgColor.r, bgColor.g, bgColor.b);
        context->re(x, y - height, width, height);
        context->f();
        context->rg(0, 0, 0);
    }
    else
    {
        XObjectContentContext *xobjectContentContext = FormXObject->GetContentContext();
        xobjectContentContext->rg(bgColor.r, bgColor.g, bgColor.b);
        xobjectContentContext->re(x, y - height, width, height);
        xobjectContentContext->f();
        xobjectContentContext->rg(0, 0, 0);
    }
}

void PDFTable::DrawCellBorder(PDFFormXObject *FormXObject, PageContentContext *context,
                              double x, double y, double width, double height, double borderWidth, double topBorderWidth,double bottomBorderWidth,double leftBorderWidth,double rightBorderWidth)
{
    if (borderWidth == 0 && topBorderWidth==0 && bottomBorderWidth==0 && leftBorderWidth==0 && rightBorderWidth==0)
        return;

    if(borderWidth>0)
    {
        pdf->addRectangle(FormXObject, x,  y- height, width, height,
                                   -1.0, -1.0, -1.0,
                                   mStyle.borderColor.r, mStyle.borderColor.g, mStyle.borderColor.b,
                                   borderWidth);        
    }
    if(topBorderWidth>0)
    {
        double startX = x;
        double startY = y;
        double endX = x + width;
        double endY = y;
        pdf->addLine(FormXObject, startX, startY, endX, endY,
                              topBorderWidth, mStyle.borderColor.r, mStyle.borderColor.g, mStyle.borderColor.b);
    }
    if(bottomBorderWidth>0)
    {
        double startX = x;
        double startY = y - height;
        double endX = x + width;
        double endY = y - height;
        pdf->addLine(FormXObject, startX, startY, endX, endY,
                              topBorderWidth, mStyle.borderColor.r, mStyle.borderColor.g, mStyle.borderColor.b);
    }
    if(leftBorderWidth>0)
    {
        double startX = x;
        double startY = y;
        double endX = x;
        double endY = y - height;
        pdf->addLine(FormXObject, startX, startY, endX, endY,
                              topBorderWidth, mStyle.borderColor.r, mStyle.borderColor.g, mStyle.borderColor.b);
    }
    if(rightBorderWidth>0)
    {
        double startX = x + width;
        double startY = y;
        double endX = x + width;
        double endY = y- height;
        pdf->addLine(FormXObject, startX, startY, endX, endY,
                              topBorderWidth, mStyle.borderColor.r, mStyle.borderColor.g, mStyle.borderColor.b);
    }

}

void PDFTable::DrawCellContent(PDFFormXObject *FormXObject, PageContentContext *context, const TableCell &cell,
                               double x, double y, double width, double height)
{

    if (cell.content.empty())
        return;

    TableStyle::Color textColor = cell.textColor.r >= 0 ? cell.textColor : mStyle.textColor;

    double fontSize = cell.fontSize > 0 ? cell.fontSize : mStyle.fontSize;

    std::shared_ptr<PDFUsedFont> fontID;
    if (!cell.font)
    {
        fontID = mStyle.font;
    }
    else
    {
        fontID = cell.font;
    }

    /*AbstractContentContext::TextOptions textOptions(
        fontID.get(),
        fontSize
    );*/

    // Calculate text position based on HAlignment
    auto dimensions = fontID->CalculateTextDimensions(cell.content, fontSize);

    double textX = x + mStyle.cellPadding;
    double textY = y - mStyle.cellPadding;

    //std::cout<<cell.content<<"  =>   x:"<<x<<"    y:"<<y<<"    textX:"<<textX<<"   textY:"<<textY<<"    cellPading:"<<mStyle.cellPadding<<"    height:"<<height<<"    width:"<<width<<"     Final height:"<<height- (mStyle.cellPadding*2)<<"     Final Width:"<<width- (mStyle.cellPadding*2)<<std::endl;
   
    pdf->addText(FormXObject,textX,textY,cell.content,fontID,fontSize,textColor.r, textColor.g, textColor.b,cell.hAlignment,cell.vAlignment,width- (mStyle.cellPadding*2),height- (mStyle.cellPadding*2),10,false);
    
}

double PDFTable::GetHeaderHeight(const std::vector<TableRow> &rows)
{
    double headerHeight = 0;
    for (const auto &row : rows)
    {
        if (row.isHeader)
        {
            headerHeight += row.cells[0]->height;
        }
    }
    return headerHeight;
}

void PDFTable::DrawTableHeader(PDFFormXObject *FormXObject, PageContentContext *context,
                               const std::vector<TableRow> &rows,
                               const std::vector<CellPosition> &cellPositions,
                               double startX, double startY, double tableWidth)
{
    double currentY = startY;
    for (int rowIdx = 0; rowIdx < static_cast<int>(rows.size()); ++rowIdx)
    {
        const auto &row = rows[rowIdx];
        double rowHeight = row.cells[0]->height;
        if (row.isHeader)
        {
            DrawRowCells(FormXObject, context, cellPositions, rowIdx, startX, currentY, tableWidth);
            currentY -= rowHeight;
        }
    }
}

PDFCreator::PDFCreator(double width, double height, double margin, double headerHeight, double footerHeight)
    : currentPage(nullptr), currentContext(nullptr), pageWidth(width), pageHeight(height), initPageFunc([this](int x)
                                                                                                        { std::cout << "Page: " << x << std::endl; })
{
    pageStyle.margin = margin;
    pageStyle.headerHeight = headerHeight;
    pageStyle.footerHeight = footerHeight;
    pageNumber = 0;
}

PDFCreator::~PDFCreator()
{
    clearImageCache();
    closeDocument();
}

bool PDFCreator::createDocument(const std::string &filename)
{
    EStatusCode status = pdfWriter.StartPDF(filename, ePDFVersion13);
    if (status != eSuccess)
    {
        std::cerr << "Failed to create PDF document: " << filename << std::endl;
        return false;
    }
    currentFilename = filename;
    std::cout << "PDF document created: " << filename << std::endl;
    setFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

    return true;
}

bool PDFCreator::openDocument(const std::string &filename)
{

    EStatusCode status = pdfWriter.ModifyPDF(filename, ePDFVersion13, "");
    if (status != eSuccess)
    {
        std::cerr << "Failed to create PDF document: " << filename << std::endl;
        return false;
    }
    currentFilename = filename;
    std::cout << "PDF document created: " << filename << std::endl;
    setFont();

    return true;
}

bool PDFCreator::saveDocument()
{
    if (currentContext)
    {
        pdfWriter.EndPageContentContext(currentContext);
        currentContext = nullptr;
    }

    if (currentPage)
    {
        pdfWriter.WritePageAndRelease(currentPage);
        currentPage = nullptr;
    }

    EStatusCode status = pdfWriter.EndPDF();
    if (status == eSuccess)
    {
        std::cout << "PDF document saved: " << currentFilename << std::endl;
        return true;
    }
    else
    {
        std::cerr << "Failed to save PDF document" << std::endl;
        return false;
    }
}

void PDFCreator::closeDocument()
{

    if (currentContext)
    {
        pdfWriter.EndPageContentContext(currentContext);
        currentContext = nullptr;
    }

    if (currentPage)
    {
        pdfWriter.WritePageAndRelease(currentPage);
        currentPage = nullptr;
    }
}

void PDFCreator::createNewPage()
{
    // Finalize current page if exists
    if (currentContext)
    {
        pdfWriter.EndPageContentContext(currentContext);
        currentContext = nullptr;
    }

    if (currentPage)
    {
        pdfWriter.WritePageAndRelease(currentPage);
        currentPage = nullptr;
    }

    // Create new page
    currentPage = new PDFPage();
    currentPage->SetMediaBox(PDFRectangle(0, 0, pageWidth, pageHeight));

    currentContext = pdfWriter.StartPageContentContext(currentPage);
    if (!currentContext)
    {
        std::cerr << "Failed to create page content context" << std::endl;
        delete currentPage;
        currentPage = nullptr;
        return;
    }

    pageNumber++;
    if (initPageFunc)
        initPageFunc(pageNumber);
}

PDFFormXObject *PDFCreator::createHeader()
{
    PDFFormXObject *formXObject;
    formXObject = pdfWriter.StartFormXObject(PDFRectangle(0, 0, pageWidth, pageStyle.headerHeight));
    if (!formXObject)
    {
        return nullptr;
    }

    return formXObject;
}

ObjectIDType PDFCreator::closeXObject(PDFFormXObject *formXObject)
{
    if (formXObject)
    {
        auto formObjectID = formXObject->GetObjectID();
        if (pdfWriter.EndFormXObjectAndRelease(formXObject) != PDFHummus::eSuccess)
        {
            std::cout << "failed to write XObject form\n"
                      << std::endl;
            return 0;
        }
        formXObject = nullptr;
        return formObjectID;
    }

    return 0;
}

PDFFormXObject *PDFCreator::createFooter()
{
    PDFFormXObject *formXObject;
    formXObject = pdfWriter.StartFormXObject(PDFRectangle(0, 0, pageWidth, pageStyle.footerHeight));
    if (!formXObject)
    {
        return nullptr;
    }
    return formXObject;
}

void PDFCreator::setPageStyle(double margin, double headerHeight, double footerHeight)
{
    pageStyle.margin = margin;
    pageStyle.headerHeight = headerHeight;
    pageStyle.footerHeight = footerHeight;
}

void PDFCreator::setFont(const std::string &fontPath)
{
    font = getFontByPath(fontPath);
    return;
}

std::shared_ptr<PDFUsedFont> PDFCreator::getFontByPath(const std::string &fontPath)
{
    auto it = fontCache.find(fontPath);
    if (it != fontCache.end())
    {
        return it->second;
    }
    PDFUsedFont *rawFont = pdfWriter.GetFontForFile(fontPath);
    if (!rawFont)
    {
        throw std::runtime_error("Failed to load font: " + fontPath);
    }

    auto sharedFont = std::shared_ptr<PDFUsedFont>(rawFont,
                                                   [](PDFUsedFont *font) {});
    fontCache.emplace(fontPath, sharedFont);
    return sharedFont;
};

void PDFCreator::addHeader(ObjectIDType FormXObjectId)
{
    if (!currentContext)
        return;

    currentContext->q();
    currentContext->cm(1, 0, 0, 1, pageStyle.margin, pageHeight - pageStyle.headerHeight);
    currentContext->Do(currentPage->GetResourcesDictionary().AddFormXObjectMapping(FormXObjectId));
    currentContext->Q();
}

void PDFCreator::addFooter(ObjectIDType FormXObjectId)
{

    if (!currentContext)
        return;
    currentContext->q();
    currentContext->cm(1, 0, 0, 1, pageStyle.margin, pageStyle.footerHeight);
    currentContext->Do(currentPage->GetResourcesDictionary().AddFormXObjectMapping(FormXObjectId));
    currentContext->Q();
}

Dimension PDFCreator::addText(PDFFormXObject *FormXObject, double x, double y, const std::string &text,std::shared_ptr<PDFUsedFont> textFont, double fontSize,
                              double r, double g, double b, HAlignment hAlignment,VAlignment vAlignment, double maxWidth,double maxheight, double lineSpace, bool isHidden)
{
    Dimension ret;
    ret.x = x;
    ret.y = y;

    if (!currentContext)
        return ret;

     std::shared_ptr<PDFUsedFont> fontID;
    if (!textFont)
    {
        fontID = font;
    }
    else
    {
        fontID = textFont;
    }

    double maxWidth_ = pageWidth;
    double maxHeight_ = pageHeight;
    if (maxWidth > 0)
        maxWidth_ = maxWidth;

    if (maxheight > 0)
        maxHeight_ = maxheight;


    AdvancedTextWrapper wrapper(fontID, fontSize, lineSpace);
    AdvancedTextWrapper::WrappingOptions options;

    options.maxWidth = maxWidth_;
    options.alignment = hAlignment;
    options.hyphenate = true;
    auto result = wrapper.wrapText(text, options);
    auto result1 = wrapper.wrapText("X", options);
    double currentY = y;
    double maxLineWidth = 0;
    switch (vAlignment)
    {
        case VAlignment::CENTER:
            currentY = y - (maxHeight_ - result.totalHeight) / 2-(result1.totalHeight);
            break;
        case VAlignment::TOP:
            currentY = y -result1.totalHeight;
            break;
        case VAlignment::BOTTOM:
        default:
            currentY = y-maxHeight_+ result.totalHeight -result1.totalHeight;
            break;
    }
    //std::cout<<" result.totalHeight:"<<result.totalHeight<<" result1.totalHeight:"<<result1.totalHeight<<"         currentY: "<<currentY<<std::endl;
    if (FormXObject == nullptr)
    {
        currentContext->BT();
        currentContext->k(r, g, b, 1);
        currentContext->Tf(fontID.get(), fontSize);
        for (size_t i = 0; i < result.lines.size(); ++i)
        {
            if (result.lineWidths[i] > maxLineWidth)
                maxLineWidth = result.lineWidths[i];
            double xPosition = x;
            double yPosition = y;
            switch (hAlignment)
            {
                case HAlignment::CENTER:
                    xPosition = x + (maxWidth_ - result.lineWidths[i]) / 2;
                    break;
                case HAlignment::RIGHT:
                    xPosition = x + (maxWidth_ - result.lineWidths[i]);
                    break;
                case HAlignment::LEFT:
                default:
                    xPosition = x;
                    break;
            }
            
            if (!isHidden)
            {
                currentContext->Tm(1, 0, 0, 1, xPosition, currentY);
                currentContext->Tj(result.lines[i]);
            }
            currentY -= result.lineHeight + lineSpace;
        }
        currentContext->ET();
    }
    else
    {
        XObjectContentContext *xobjectContentContext = FormXObject->GetContentContext();
        xobjectContentContext->BT();
        xobjectContentContext->k(r, g, b, 1);
        xobjectContentContext->Tf(fontID.get(), fontSize);
        for (size_t i = 0; i < result.lines.size(); ++i)
        {
            if (result.lineWidths[i] > maxLineWidth)
                maxLineWidth = result.lineWidths[i];
            double xPosition = x;
            switch (hAlignment)
            {
            case HAlignment::CENTER:
                xPosition = x + (maxWidth_ - result.lineWidths[i]) / 2;
                break;
            case HAlignment::RIGHT:
                xPosition = x + (maxWidth_ - result.lineWidths[i]);
                break;
            case HAlignment::LEFT:
            default:
                xPosition = x;
                break;
            }
            if (!isHidden)
            {
                xobjectContentContext->Tm(1, 0, 0, 1, xPosition, currentY);
                xobjectContentContext->Tj(result.lines[i]);
            }
            currentY -= result.lineHeight + lineSpace;
        }
        xobjectContentContext->ET();
    }
    ret.height = y - currentY;
    ret.width = maxLineWidth;
    ret.ok = true;
    return ret;
}

Dimension PDFCreator::addLine(PDFFormXObject *FormXObject, double startX, double startY, double endX, double endY,
                              double lineWidth, double r, double g, double b)
{
    Dimension ret;
    double minX = std::min({startX, endX});
    double maxX = std::max({startX, endX});
    ret.width = maxX - minX;
    ret.x = minX;

    double minY = std::min({startY, endY});
    double maxY = std::max({startY, endY});
    ret.height = maxY - minY;
    ret.y = minY;

    if (!currentContext)
        return ret;

    if (FormXObject == nullptr)
    {
        currentContext->rg(r, g, b);
        currentContext->w(lineWidth);
        currentContext->m(startX, startY);
        currentContext->l(endX, endY);
        currentContext->S();
        currentContext->w(1);
        currentContext->rg(0, 0, 0);
    }
    else
    {
        XObjectContentContext *xobjectContentContext = FormXObject->GetContentContext();
        xobjectContentContext->rg(r, g, b);
        xobjectContentContext->w(lineWidth);
        xobjectContentContext->m(startX, startY);
        xobjectContentContext->l(endX, endY);
        xobjectContentContext->S();
        xobjectContentContext->w(1);
        xobjectContentContext->rg(0, 0, 0);
    }
    ret.ok = true;
    return ret;
}

Dimension PDFCreator::addHorizontalLine(PDFFormXObject *FormXObject, double y, double lineWidth)
{
    return addLine(FormXObject, pageStyle.margin, y, pageWidth - pageStyle.margin, y, lineWidth);
}

Dimension PDFCreator::addVerticalLine(PDFFormXObject *FormXObject, double x, double lineWidth)
{

    return addLine(FormXObject, x, pageStyle.footerHeight, x, pageHeight - pageStyle.headerHeight, lineWidth);
}

Dimension PDFCreator::addRectangle(PDFFormXObject *FormXObject, double x, double y, double width, double height,
                                   double fillR, double fillG, double fillB,
                                   double strokeR, double strokeG, double strokeB,
                                   double lineWidth)
{
    Dimension ret;
    ret.x = x;
    ret.y = y;
    ret.width = width;
    ret.height = height;

    if (!currentContext)
        return ret;

    if (FormXObject == nullptr)
    {
        currentContext->w(lineWidth);

        // Fill if color specified
        if (fillR >= 0 && fillG >= 0 && fillB >= 0)
        {
            currentContext->rg(fillR, fillG, fillB);
            currentContext->re(x, y, width, height);
            currentContext->f();
        }

        // Stroke
        currentContext->rg(strokeR, strokeG, strokeB);
        currentContext->re(x, y, width, height);
        currentContext->S();

        currentContext->rg(0, 0, 0);
        currentContext->w(1); // Reset line width
    }
    else
    {
        XObjectContentContext *xobjectContentContext = FormXObject->GetContentContext();
        xobjectContentContext->w(lineWidth);

        // Fill if color specified
        if (fillR >= 0 && fillG >= 0 && fillB >= 0)
        {
            xobjectContentContext->rg(fillR, fillG, fillB);
            xobjectContentContext->re(x, y, width, height);
            xobjectContentContext->f();
        }

        // Stroke
        xobjectContentContext->rg(strokeR, strokeG, strokeB);
        xobjectContentContext->re(x, y, width, height);
        xobjectContentContext->S();
        xobjectContentContext->rg(0, 0, 0);
        xobjectContentContext->w(1); // Reset line width
    }
    ret.ok = true;
    return ret;
}

Dimension PDFCreator::addCircle(PDFFormXObject *FormXObject, double centerX, double centerY, double radius,
                                double fillR, double fillG, double fillB,
                                double strokeR, double strokeG, double strokeB,
                                double lineWidth)
{
    Dimension ret;
    ret.x = centerX - radius;
    ret.y = centerY - radius;
    ret.width = radius * 2;
    ret.height = ret.width;

    if (!currentContext)
        return ret;

    XObjectContentContext *xobjectContentContext;
    if (FormXObject != nullptr)
        xobjectContentContext = FormXObject->GetContentContext();

    if (FormXObject == nullptr)
    {
        currentContext->w(lineWidth);
        if (fillR >= 0 && fillG >= 0 && fillB >= 0)
        {
            currentContext->rg(fillR, fillG, fillB);
            drawCircle(FormXObject, currentContext, centerX, centerY, radius);
            currentContext->f();
        }

        currentContext->rg(strokeR, strokeG, strokeB);
        drawCircle(FormXObject, currentContext, centerX, centerY, radius);
        currentContext->S();
        currentContext->w(1);
        currentContext->rg(0, 0, 0);
    }
    else
    {
        xobjectContentContext->w(lineWidth);
        if (fillR >= 0 && fillG >= 0 && fillB >= 0)
        {
            xobjectContentContext->rg(fillR, fillG, fillB);
            drawCircle(FormXObject, currentContext, centerX, centerY, radius);
            xobjectContentContext->f();
        }

        xobjectContentContext->rg(strokeR, strokeG, strokeB);
        drawCircle(FormXObject, currentContext, centerX, centerY, radius);
        xobjectContentContext->S();
        xobjectContentContext->w(1);
        xobjectContentContext->rg(0, 0, 0);
    }
    ret.ok = true;
    return ret;
}

void PDFCreator::drawCircle(PDFFormXObject *FormXObject, PageContentContext *context, double centerX, double centerY, double radius)
{
    const double magic = 0.551784;
    double m = radius * magic;

    if (FormXObject == nullptr)
    {

        context->m(centerX, centerY + radius);
        context->c(centerX + m, centerY + radius,
                   centerX + radius, centerY + m,
                   centerX + radius, centerY);
        context->c(centerX + radius, centerY - m,
                   centerX + m, centerY - radius,
                   centerX, centerY - radius);
        context->c(centerX - m, centerY - radius,
                   centerX - radius, centerY - m,
                   centerX - radius, centerY);
        context->c(centerX - radius, centerY + m,
                   centerX - m, centerY + radius,
                   centerX, centerY + radius);
        context->h();
    }
    else
    {
        XObjectContentContext *xobjectContentContext = FormXObject->GetContentContext();
        xobjectContentContext->m(centerX, centerY + radius);
        xobjectContentContext->c(centerX + m, centerY + radius,
                                 centerX + radius, centerY + m,
                                 centerX + radius, centerY);
        xobjectContentContext->c(centerX + radius, centerY - m,
                                 centerX + m, centerY - radius,
                                 centerX, centerY - radius);
        xobjectContentContext->c(centerX - m, centerY - radius,
                                 centerX - radius, centerY - m,
                                 centerX - radius, centerY);
        xobjectContentContext->c(centerX - radius, centerY + m,
                                 centerX - m, centerY + radius,
                                 centerX, centerY + radius);
        xobjectContentContext->h();
    }
}

Dimension PDFCreator::addTriangle(PDFFormXObject *FormXObject, double x1, double y1, double x2, double y2, double x3, double y3,
                                  double fillR, double fillG, double fillB,
                                  double strokeR, double strokeG, double strokeB,
                                  double lineWidth)
{
    Dimension ret;
    double minX = std::min({x1, x2, x3});
    double maxX = std::max({x1, x2, x3});
    ret.width = maxX - minX;
    ret.x = minX;

    // Calculate height (max y - min y)
    double minY = std::min({y1, y2, y3});
    double maxY = std::max({y1, y2, y3});
    ret.height = maxY - minY;
    ret.y = minY;

    if (!currentContext)
        return ret;

    if (FormXObject == nullptr)
    {
        currentContext->w(lineWidth);

        // Fill if color specified
        if (fillR >= 0 && fillG >= 0 && fillB >= 0)
        {
            currentContext->rg(fillR, fillG, fillB);
            currentContext->m(x1, y1);
            currentContext->l(x2, y2);
            currentContext->l(x3, y3);
            currentContext->h();
            currentContext->f();
        }

        // Stroke
        currentContext->rg(strokeR, strokeG, strokeB);
        currentContext->m(x1, y1);
        currentContext->l(x2, y2);
        currentContext->l(x3, y3);
        currentContext->h();
        currentContext->S();
        currentContext->rg(0, 0, 0);
        currentContext->w(1);
    }
    else
    {
        XObjectContentContext *xobjectContentContext = FormXObject->GetContentContext();
        xobjectContentContext->w(lineWidth);

        // Fill if color specified
        if (fillR >= 0 && fillG >= 0 && fillB >= 0)
        {
            xobjectContentContext->rg(fillR, fillG, fillB);
            xobjectContentContext->m(x1, y1);
            xobjectContentContext->l(x2, y2);
            xobjectContentContext->l(x3, y3);
            xobjectContentContext->h();
            xobjectContentContext->f();
        }

        // Stroke
        xobjectContentContext->rg(strokeR, strokeG, strokeB);
        xobjectContentContext->m(x1, y1);
        xobjectContentContext->l(x2, y2);
        xobjectContentContext->l(x3, y3);
        xobjectContentContext->h();
        xobjectContentContext->S();
        xobjectContentContext->w(1);
        xobjectContentContext->rg(0, 0, 0);
    }
    ret.ok = true;
    return ret;
}

Dimension PDFCreator::DrawTableOnPages(PDFFormXObject *FormXObject, std::shared_ptr<PDFTable> table,
                                       const std::vector<TableRow> &rows,
                                       const std::vector<CellPosition> &cellPositions,
                                       double startX, double tableWidth)
{
    Dimension ret;
    ret.x = startX;
    ret.y = table->mCurrentY;
    int currentRow = 0;
    bool isFirstPage = true;

    while (currentRow < static_cast<int>(rows.size()))
    {
        double currentPageY = table->mCurrentY;
        if (!isFirstPage)
        {
            currentPageY = pageHeight - pageStyle.margin - pageStyle.headerHeight - 50;
            createNewPage();
        }
        if (!currentContext)
            return ret;

        if (!isFirstPage)
        {
            table->DrawTableHeader(FormXObject, currentContext, rows, cellPositions, startX, currentPageY, tableWidth);
            currentPageY -= table->GetHeaderHeight(rows);
        }

        int rowsDrawn = table->DrawRowsOnPage(FormXObject, currentContext, rows, cellPositions,
                                              startX, currentPageY, tableWidth, currentRow);

        if (rowsDrawn == 0)
        {
            // Couldn't fit any rows, need to handle this case
            break;
        }

        currentRow += rowsDrawn;

        currentPageY = pageHeight - 50; // Reset for next page
        if(FormXObject==nullptr)  isFirstPage = false;
    }
    ret.ok = true;
    return ret;
}

Dimension PDFCreator::DrawTableWithPageBreaks(PDFFormXObject *FormXObject, std::shared_ptr<PDFTable> table, const std::vector<TableRow> &rows,
                                              double startX, double startY,
                                              double tableWidth)
{
    Dimension ret;
    ret.ok = true;
    if (rows.empty())
        return ret;

    table->mCurrentY = startY;

    auto cellGrid = table->BuildCellGrid(rows);
    auto cellPositions = table->CalculateCellPositions(cellGrid, tableWidth);

    return DrawTableOnPages(FormXObject, table, rows, cellPositions, startX, tableWidth);
}

std::shared_ptr<PDFTable> PDFCreator::CreateTable()
{
    std::shared_ptr<PDFTable> table = std::make_shared<PDFTable>(this,pageStyle.margin);
    tables.push_back(table);
    return tables.back();
}

PDFImageXObject *PDFCreator::getImageXObject(const std::string &imagePath)
{
    auto it = imageCache.find(imagePath);
    if (it != imageCache.end())
    {
        return it->second;
    }

    // Create new image XObject
    PDFImageXObject *imageXObject = pdfWriter.CreateImageXObjectFromJPGFile(imagePath.c_str());

    if (imageXObject)
    {
        imageCache[imagePath] = imageXObject;
        std::cout << "Loaded image: " << imagePath << std::endl;
    }
    else
    {
        std::cerr << "Failed to load image: " << imagePath << std::endl;
    }

    return imageXObject;
}

Dimension PDFCreator::embedImage(PDFFormXObject *FormXObject, const std::string &imagePath, double x, double y, double width, double height, double scale, double angle, int index)
{
    Dimension ret;
    ret.x = x;
    ret.y = y;
    if (!currentPage || !currentContext)
    {
        std::cerr << "No active page or context" << std::endl;
        return ret;
    }
    XObjectContentContext *xobjectContentContext;
    if (FormXObject != nullptr)
    {
        xobjectContentContext = FormXObject->GetContentContext();
    }
    AbstractContentContext::ImageOptions opt;
    if (index > 0)
        opt.imageIndex = index;
    if (scale > 0 || angle > 0)
    {
        opt.transformationMethod = AbstractContentContext::eMatrix;
        double s = 1;
        if (scale > 0)
        {
            opt.matrix[0] = opt.matrix[3] = scale;
            s = scale;
        }
        if (angle > 0)
        {
            double a = angle * (3.14159 / 180);
            opt.matrix[0] = cos(angle) * s;
            opt.matrix[1] = sin(angle) * s;
            opt.matrix[2] = -sin(angle) * s;
            opt.matrix[3] = cos(angle) * s;
        }
    }
    else if (width > 0 && height > 0)
    {
        opt.transformationMethod = AbstractContentContext::eFit;
        opt.boundingBoxHeight = height;
        opt.boundingBoxWidth = width;
        opt.fitProportional = true;
    }
    if (FormXObject == nullptr)
    {
        currentContext->DrawImage(x, y, imagePath, opt);
    }
    else
    {
        xobjectContentContext->DrawImage(x, y, imagePath, opt);
    }
    ret.ok = true;
    return ret;
}

void PDFCreator::getImageDimensions(const std::string &imagePath, double &width, double &height)
{
    DoubleAndDoublePair jpgDimensions = pdfWriter.GetImageDimensions(imagePath);
    width = jpgDimensions.first;
    height = jpgDimensions.second;
}

void PDFCreator::clearImageCache()
{
    for (auto &pair : imageCache)
    {
        delete pair.second;
    }
    imageCache.clear();
}

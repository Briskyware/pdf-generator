#include "BriskyPdfJson.h"
#include <iostream>

int main()
{
    std::shared_ptr<PDFCreator> pdf;
    PDFJson parser(pdf);
    if (parser.processFromFile("Sample.json")) {
        std::cout << "JSON processed successfully!" << std::endl;
    }
    return 0;
}

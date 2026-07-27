//------------ kuvbur 2026 ------------
// LayoutOptimizer Debug Export - экспорт всей геометрии выносок и размеров в CSV
// Для отладки без запуска Archicad - собирает все поля API_LabelType / API_DimensionType

#include "ACAPinc.h"
#include "APIEnvir.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace LayoutOptimizer {

// Вспомогательная: конвертация GUID в строку
static GS::UniString GuidToString (const API_Guid& guid) {
    return APIGuidToString (guid);
}

// Экспорт всех Label элементов
GSErrCode ExportLabelsGeometryToCSV (const char* filename) {
    std::ofstream csv (filename);
    if (!csv.is_open ())
        return Error;

    csv << "guid,typeID,"
        << "begC_x,begC_y,midC_x,midC_y,endC_x,endC_y,"
        << "anchorPoint,leaderShape,labelClass,"
        << "textLoc_x,textLoc_y,textAngle,"
        << "textWidth,textHeight,textContent\n";

    GS::Array<API_Guid> guids;
    GSErrCode err = ACAPI_Element_GetElemList (API_LabelID, &guids);
    if (err != NoError)
        return err;

    for (UInt32 i = 0; i < guids.GetSize (); ++i) {
        const API_Guid& guid = guids[i];
        API_Element elem;
        BNZeroMemory (&elem, sizeof (API_Element));
        elem.header.guid = guid;

        err = ACAPI_Element_Get (&elem);
        if (err != NoError)
            continue;

        API_ElementMemo memo;
        BNZeroMemory (&memo, sizeof (API_ElementMemo));
        // AC25: Без маски (0) = получить все данные memo, включая Unicode paragraphs
        ACAPI_Element_GetMemo (guid, &memo, 0);

        double textWidth = 50.0;
        double textHeight = 3.0;
        GS::UniString textContent;
        
        if (memo.paragraphs != nullptr && memo.paragraphs[0] != nullptr) {
            const API_ParagraphType& para = memo.paragraphs[0][0];
            textWidth = para.width;
            textHeight = para.height;
        } else if (elem.header.typeID == API_LabelID) {
            // Для custom label классов: оценка размера из textSize элемента
            double fontSizeMm = elem.label.textSize > 0.0 ? elem.label.textSize : 2.5;
            textHeight = fontSizeMm * 1.2;
            textWidth = 8.0 * fontSizeMm * 0.6;  // 8 символов * ширина символа
        }
        if (memo.textContent != nullptr && memo.textContent[0] != nullptr) {
            textContent = memo.textContent[0];
        }

        GS::UniString guidStr = GuidToString (guid);

        csv << std::fixed << std::setprecision (4);
        csv << guidStr.ToCStr().Get() << ",label,"
            << elem.label.begC.x << "," << elem.label.begC.y << ","
            << elem.label.midC.x << "," << elem.label.midC.y << ","
            << elem.label.endC.x << "," << elem.label.endC.y << ","
            << (int)elem.label.anchorPoint << ","
            << (int)elem.label.leaderShape << ","
            << (int)elem.label.labelClass << ","
            << elem.label.u.text.loc.x << "," << elem.label.u.text.loc.y << ","
            << elem.label.u.text.angle << ","
            << textWidth << "," << textHeight << ","
            << textContent.ToCStr().Get() << "\n";

        ACAPI_DisposeElemMemoHdls (&memo);
    }

    csv.close ();
    return NoError;
}

// Экспорт всех Dimension элементов
GSErrCode ExportDimensionsGeometryToCSV (const char* filename) {
    std::ofstream csv (filename);
    if (!csv.is_open ())
        return Error;

    csv << "guid,typeID,"
        << "refC_x,refC_y,direction_x,direction_y,"
        << "defNote_x,defNote_y,defNote_angle,"
        << "textWidth,textHeight,textContent\n";

    GS::Array<API_Guid> guids;
    GSErrCode err = ACAPI_Element_GetElemList (API_DimensionID, &guids);
    if (err != NoError)
        return err;

    for (UInt32 i = 0; i < guids.GetSize (); ++i) {
        const API_Guid& guid = guids[i];
        API_Element elem;
        BNZeroMemory (&elem, sizeof (API_Element));
        elem.header.guid = guid;

        err = ACAPI_Element_Get (&elem);
        if (err != NoError)
            continue;

        API_ElementMemo memo;
        BNZeroMemory (&memo, sizeof (API_ElementMemo));
        // AC25: Без маски (0) = получить все данные memo, включая Unicode paragraphs
        ACAPI_Element_GetMemo (guid, &memo, 0);

        double textWidth = 50.0;
        double textHeight = 3.0;
        GS::UniString textContent;
        
        if (memo.paragraphs != nullptr && memo.paragraphs[0] != nullptr) {
            const API_ParagraphType& para = memo.paragraphs[0][0];
            textWidth = para.width;
            textHeight = para.height;
        } else if (elem.header.typeID == API_LabelID) {
            // Для custom label классов: оценка размера из textSize элемента
            double fontSizeMm = elem.label.textSize > 0.0 ? elem.label.textSize : 2.5;
            textHeight = fontSizeMm * 1.2;
            textWidth = 8.0 * fontSizeMm * 0.6;  // 8 символов * ширина символа
        }
        if (memo.textContent != nullptr && memo.textContent[0] != nullptr) {
            textContent = memo.textContent[0];
        }

        GS::UniString guidStr = GuidToString (guid);

        csv << std::fixed << std::setprecision (4);
        csv << guidStr.ToCStr().Get() << ",dimension,"
            << elem.dimension.refC.x << "," << elem.dimension.refC.y << ","
            << elem.dimension.direction.x << "," << elem.dimension.direction.y << ","
            << elem.dimension.defNote.pos.x << "," << elem.dimension.defNote.pos.y << ","
            << elem.dimension.defNote.noteAngle << ","
            << textWidth << "," << textHeight << ","
            << textContent.ToCStr().Get() << "\n";

        ACAPI_DisposeElemMemoHdls (&memo);
    }

    csv.close ();
    return NoError;
}

// Экспорт препятствий (locked elements)
GSErrCode ExportObstaclesToCSV (const char* filename) {
    std::ofstream csv (filename);
    if (!csv.is_open ())
        return Error;

    csv << "guid,type,locked,x1,y1,x2,y2,x3,y3,x4,y4\n";

    // Locked labels
    GS::Array<API_Guid> guids;
    GSErrCode err = ACAPI_Element_GetElemList (API_LabelID, &guids);
    if (err == NoError) {
        for (UInt32 i = 0; i < guids.GetSize (); ++i) {
            const API_Guid& guid = guids[i];
            API_Element elem;
            BNZeroMemory (&elem, sizeof (API_Element));
            elem.header.guid = guid;

            err = ACAPI_Element_Get (&elem);
            if (err != NoError)
                continue;

            bool locked = (elem.header.floorInd & 0x8000) != 0;
            if (!locked) {
                API_Attribute layerAttr;
                BNZeroMemory (&layerAttr, sizeof (API_Attribute));
                layerAttr.header.typeID = API_LayerID;
                layerAttr.header.index = elem.header.layer;
                if (ACAPI_Attribute_Get (&layerAttr) == NoError) {
                    locked = (layerAttr.layer.head.flags & 1) != 0;
                }
            }

            if (locked) {
                GS::UniString guidStr = GuidToString (guid);
                csv << std::fixed << std::setprecision (4);
                csv << guidStr.ToCStr().Get() << ",label,1,"
                    << elem.label.begC.x << "," << elem.label.begC.y << ","
                    << elem.label.midC.x << "," << elem.label.midC.y << ","
                    << elem.label.endC.x << "," << elem.label.endC.y << ","
                    << elem.label.begC.x << "," << elem.label.begC.y << "\n";
            }
        }
    }

    // Locked dimensions
    GSErrCode err2 = ACAPI_Element_GetElemList (API_DimensionID, &guids);
    if (err2 == NoError) {
        for (UInt32 i = 0; i < guids.GetSize (); ++i) {
            const API_Guid& guid = guids[i];
            API_Element elem;
            BNZeroMemory (&elem, sizeof (API_Element));
            elem.header.guid = guid;

            err = ACAPI_Element_Get (&elem);
            if (err != NoError)
                continue;

            bool locked = (elem.header.floorInd & 0x8000) != 0;
            if (!locked) {
                API_Attribute layerAttr;
                BNZeroMemory (&layerAttr, sizeof (API_Attribute));
                layerAttr.header.typeID = API_LayerID;
                layerAttr.header.index = elem.header.layer;
                if (ACAPI_Attribute_Get (&layerAttr) == NoError) {
                    locked = (layerAttr.layer.head.flags & 1) != 0;
                }
            }

            if (locked) {
                GS::UniString guidStr = GuidToString (guid);
                csv << std::fixed << std::setprecision (4);
                csv << guidStr.ToCStr().Get() << ",dimension,1,"
                    << elem.dimension.refC.x << "," << elem.dimension.refC.y << ","
                    << elem.dimension.defNote.pos.x << "," << elem.dimension.defNote.pos.y << ","
                    << elem.dimension.defNote.pos.x << "," << elem.dimension.defNote.pos.y << ","
                    << elem.dimension.refC.x << "," << elem.dimension.refC.y << "\n";
            }
        }
    }

    csv.close ();
    return NoError;
}

// Главная функция экспорта всего
GSErrCode RunFullDebugExport () {
    GSErrCode err = ExportLabelsGeometryToCSV ("debug_labels.csv");
    if (err != NoError) return err;

    err = ExportDimensionsGeometryToCSV ("debug_dimensions.csv");
    if (err != NoError) return err;

    err = ExportObstaclesToCSV ("debug_obstacles.csv");
    return err;
}

} // namespace LayoutOptimizer
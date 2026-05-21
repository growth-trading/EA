#include <ChartObjects\ChartObjectsTxtControls.mqh>
#include <Trade\Trade.mqh>

CTrade Trade;

CChartObjectButton btnSLBE, btnAlertCheck;

CChartObjectText txtTimeCountDown;

// Input parameters
input group "=== CHUNG ===";
input double InpMaxLossAmount = 15.0;             // Số tiền rủi ro tối đa trên mỗi lệnh (Ví dụ: 100$)
input int FIVE = 50;                              // Số pip để đặt SL/TP cách điểm hòa vốn
input group "=== MÀU CHỮ ===";
input color CountdownColor = clrBlack;            // Màu sắc chữ đếm ngược
input group "=== THỜI GIAN ===";
input int InpGMTOffset = 7;                       // Chênh lệch múi giờ so với GMT
input group "=== THIẾT LẬP LỆNH ===";
input double InpDefaultLots = 0.01;               // Khối lượng mặc định
input int    InpDefaultTP   = 200;                // TP mặc định (pts)
input int    InpDefaultSL   = 150;                // SL mặc định (pts)

// Global variables
datetime CandleCloseTime;       // Biến kiểm tra giá chạy 1p một lần
bool SlBeEnabled = false;       // Biến kiểm soát dời SL về điểm hòa vốn
bool AlertCheckEnabled = true;  // Biến kiểm soát kiểm tra giá với EMA
string g_lastOrderType     = "";
string g_pendingOrderType  = "";
double g_pendingEntryPrice = 0;
double g_pendingTPPrice    = 0;
double g_pendingSLPrice    = 0;
double g_pendingTPPts      = 0;
double g_pendingSLPts      = 0;
double g_pendingLots       = 0;
string g_pendingCmt        = "";

//+------------------------------------------------------------------+
//| Hàm khởi tạo Expert Advisor                                      |
//+------------------------------------------------------------------+
int OnInit() {
    EventSetTimer(1);

    int chartW = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS);
    int chartH = (int)ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS);
    int px     = chartW - 210;
    int bx     = px + 15;
    int p2Y    = chartH - 102 - 10;

    DrawPanels();

    // Buttons bên trong Panel 2 (Control Buttons) – góc phải dưới
    if(!CreateButton(btnSLBE,       "SLBEButton",       "= BREAK EVEN",   C'0,110,50',  C'45,185,90',  bx, p2Y + 33, 170))
        return INIT_FAILED;
    if(!CreateButton(btnAlertCheck, "AlertCheckButton", "+ CHECK EMA: ON", C'20,60,150', C'80,130,230', bx, p2Y + 71, 170))
        return INIT_FAILED;

    if(!CreateText(txtTimeCountDown, "TimeCountDown", "Countdown:  s"))
        return INIT_FAILED;

    CreateOrderPanel();

    ChartRedraw(0);
    return INIT_SUCCEEDED;
}

void OnTimer() {
    // Check current time and next M1 candle close time
    datetime currentTime = TimeCurrent();
    datetime currentCandleCloseTime =
        iTime(_Symbol, PERIOD_M1, 0) + PeriodSeconds(PERIOD_M1);

    int secondsToNextCandle = (int)(currentCandleCloseTime - currentTime - 1);
    txtTimeCountDown.Description("    " + IntegerToString(secondsToNextCandle));

    txtTimeCountDown.Time(0, currentCandleCloseTime);
    txtTimeCountDown.Price(0, iClose(_Symbol, PERIOD_CURRENT, 0));

    bool isRunningEa = false;
    if(currentCandleCloseTime != CandleCloseTime &&
       currentCandleCloseTime - currentTime <= 2) {
        CandleCloseTime = currentCandleCloseTime;
        isRunningEa = true;
    }

    if(isRunningEa) {
        Draw();

        isRunningEa = false;
    }

    UpdateInfoPanel();
    CalculateForLable();
    CheckAndAdjustStopLoss();

    if(AlertCheckEnabled && _Period == PERIOD_M1) {
        CheckPriceWithEMA();
    }

    CheckNOsdCandle();
}

void OnTick() {
    if(SlBeEnabled) {
        SlBeEnabled = !SlBeEnabled;
        MoveSLBE();
    }
    UpdateOrderPreview();
}

void OnChartEvent(const int id, const long& lparam, const double& dparam,
                  const string& sparam) {
    // Cập nhật vị trí panel và nút khi chart thay đổi kích thước
    if(id == CHARTEVENT_CHART_CHANGE) {
        DrawPanels();
        int cw = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS);
        int ch = (int)ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS);
        int bx2 = cw - 210 + 15;
        int p2Y2 = ch - 102 - 10;
        ObjectSetInteger(0, "SLBEButton",       OBJPROP_XDISTANCE, bx2);
        ObjectSetInteger(0, "SLBEButton",       OBJPROP_YDISTANCE, p2Y2 + 33);
        ObjectSetInteger(0, "AlertCheckButton", OBJPROP_XDISTANCE, bx2);
        ObjectSetInteger(0, "AlertCheckButton", OBJPROP_YDISTANCE, p2Y2 + 71);
        ChartRedraw(0);
    }
    // Nhấn nút dời SL về điểm hòa vốn
    if(id == CHARTEVENT_OBJECT_CLICK && sparam == "SLBEButton") {
        SlBeEnabled = !SlBeEnabled;
    }
    // Nhấn nút kiểm tra giá với EMA
    if(id == CHARTEVENT_OBJECT_CLICK && sparam == "AlertCheckButton") {
        AlertCheckEnabled = !AlertCheckEnabled;

        if(AlertCheckEnabled) {
            btnAlertCheck.Description("+ CHECK EMA: ON");
            btnAlertCheck.BackColor(C'20,60,150');
            btnAlertCheck.BorderColor(C'80,130,230');
        } else {
            btnAlertCheck.Description("X CHECK EMA: OFF");
            btnAlertCheck.BackColor(C'145,15,15');
            btnAlertCheck.BorderColor(C'230,65,65');
        }
    }
    // Order panel buttons
    if(id == CHARTEVENT_OBJECT_CLICK && StringFind(sparam, "OP_Btn") == 0) {
        ObjectSetInteger(0, sparam, OBJPROP_STATE, false);
        if     (sparam == "OP_BtnBuy")  PrepareOrder("BUY");
        else if(sparam == "OP_BtnSell") PrepareOrder("SELL");
        else if(sparam == "OP_BtnSend" && g_pendingOrderType != "") {
            ExecuteOrderFromPanel(g_pendingOrderType);
            ClearOrderPreview();
        }
        else if(sparam == "OP_BtnCancel") {
            ClearOrderPreview();
            ObjectSetString(0, "OP_EditLots", OBJPROP_TEXT, DoubleToString(InpDefaultLots, 2));
            ObjectSetString(0, "OP_EditTP",   OBJPROP_TEXT, IntegerToString(InpDefaultTP));
            ObjectSetString(0, "OP_EditSL",   OBJPROP_TEXT, IntegerToString(InpDefaultSL));
            ObjectSetString(0, "OP_EditCmt",  OBJPROP_TEXT, "");
        }
        ChartRedraw(0);
    }
}
void OnDeinit(const int reason) {
    btnSLBE.Delete();
    btnAlertCheck.Delete();
    DeletePanels();
    DeleteOrderPanel();
    ClearOrderPreview();
    DeleteEMALines();
    ObjectsDeleteAll(0, "NOsd_",  -1, OBJ_RECTANGLE);
    ObjectsDeleteAll(0, "Price ", -1, OBJ_TREND);
    ObjectsDeleteAll(0, "TimeframeLabel_", -1, OBJ_TEXT);
    EventKillTimer();
}

void MoveSLBE() {
    // Code dời SL về điểm hòa vốn
    for(int index = PositionsTotal() - 1; index >= 0 && !IsStopped(); index--) {
        ulong ticket = PositionGetTicket(index);
        if(ticket <= 0) continue;

        if(PositionSelectByTicket(ticket)) {
            double openPrice = PositionGetDouble(POSITION_PRICE_OPEN);
            double currentTP = PositionGetDouble(POSITION_TP);
            double currentSL = PositionGetDouble(POSITION_SL);
            double profit = PositionGetDouble(POSITION_PROFIT);
            ENUM_POSITION_TYPE type =
                (ENUM_POSITION_TYPE)PositionGetInteger(POSITION_TYPE);

            double newPosition = (type == POSITION_TYPE_BUY)
                                     ? openPrice + FIVE * _Point
                                     : openPrice - FIVE * _Point;

            if(profit > 0) {
                if(!Trade.PositionModify(ticket, newPosition, currentTP)) {
                    Print("Failed to modify position #", ticket,
                          ". Error: ", GetLastError());
                }
            } else {
                if(!Trade.PositionModify(ticket, currentSL, newPosition)) {
                    Print("Failed to modify position #", ticket,
                          ". Error: ", GetLastError());
                }
            }
        }
    }
}

bool CreateButton(CChartObjectButton& button, string name, string des,
                  color bgColor, color borderClr, int x, int y, int w = 190) {
    if(!button.Create(0, name, 0, x, y, w, 25)) return false;

    button.Description(des);
    button.Color(clrWhite);
    button.BackColor(bgColor);
    button.FontSize(9);
    button.Font("Consolas");
    button.Selectable(false);
    button.BorderColor(borderClr);

    return true;
}

bool CreateLable(CChartObjectLabel& lable, string name, string des, int x,
                 int y) {
    // Tạo lable và thiết lập thuộc tính
    if(!lable.Create(0, name, 0, x, y)) return false;

    lable.Description(des);
    lable.Color(clrWhite);
    lable.Font("Calibri");
    lable.FontSize(12);

    return true;
}

bool CreateText(CChartObjectText& txtObj, string name, string des) {
    // Khởi tạo ở thời gian 0, giá 0. Sau đó sẽ di chuyển sau.
    if(!txtObj.Create(0, name, 0, 0, 0.0)) return false;

    txtObj.Description(des);
    txtObj.Color(CountdownColor);
    txtObj.Font("Calibri");
    txtObj.FontSize(12);
    txtObj.Anchor(ANCHOR_LEFT);  // Canh lề trái để chữ nằm ngay bên phải nến

    return true;
}

void DrawPanelRect(string nm, int x, int y, int w, int h, color bg, color border, int bw = 1) {
    if(ObjectFind(0, nm) < 0)
        ObjectCreate(0, nm, OBJ_RECTANGLE_LABEL, 0, 0, 0);
    ObjectSetInteger(0, nm, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
    ObjectSetInteger(0, nm, OBJPROP_XDISTANCE,   x);
    ObjectSetInteger(0, nm, OBJPROP_YDISTANCE,   y);
    ObjectSetInteger(0, nm, OBJPROP_XSIZE,       w);
    ObjectSetInteger(0, nm, OBJPROP_YSIZE,       h);
    ObjectSetInteger(0, nm, OBJPROP_BGCOLOR,     bg);
    ObjectSetInteger(0, nm, OBJPROP_BORDER_TYPE, BORDER_FLAT);
    ObjectSetInteger(0, nm, OBJPROP_COLOR,       border);
    ObjectSetInteger(0, nm, OBJPROP_WIDTH,       bw);
    ObjectSetInteger(0, nm, OBJPROP_SELECTABLE,  false);
    ObjectSetInteger(0, nm, OBJPROP_HIDDEN,      true);
}

void DrawPanelLabel(string nm, string txt, int x, int y, color clr, int fs = 10) {
    if(ObjectFind(0, nm) < 0) {
        ObjectCreate(0, nm, OBJ_LABEL, 0, 0, 0);
        ObjectSetInteger(0, nm, OBJPROP_CORNER,     CORNER_LEFT_UPPER);
        ObjectSetInteger(0, nm, OBJPROP_ANCHOR,     ANCHOR_LEFT_UPPER);
        ObjectSetString (0, nm, OBJPROP_FONT,       "Calibri");
        ObjectSetInteger(0, nm, OBJPROP_SELECTABLE, false);
        ObjectSetInteger(0, nm, OBJPROP_HIDDEN,     true);
    }
    ObjectSetInteger(0, nm, OBJPROP_XDISTANCE, x);
    ObjectSetInteger(0, nm, OBJPROP_YDISTANCE, y);
    ObjectSetInteger(0, nm, OBJPROP_COLOR,     clr);
    ObjectSetInteger(0, nm, OBJPROP_FONTSIZE,  fs);
    ObjectSetString (0, nm, OBJPROP_TEXT,      txt);
}

void DrawPanels() {
    int chartW = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS);
    int px     = chartW - 210;
    int pw     = 200;

    // ── Panel 1: Thông tin ────────────────────────────────
    DrawPanelRect("PF_P1_hdr",  px, 30, pw,  22,  C'20,60,120', C'40,80,160', 2);
    DrawPanelRect("PF_P1_body", px, 52, pw,  106, C'10,10,25',  C'40,80,140', 1);

    DrawPanelLabel("PF_P1_ttl", "  PROPFIRM BOT  v1.0",       px+4, 35,  clrWhite,     11);
    DrawPanelLabel("PF_time",   "  Time : --/--/-- --:--:--", px+4, 58,  clrSilver,    10);
    DrawPanelLabel("PF_sep1",   "  ---------------------",    px+4, 76,  C'60,60,100', 10);
    DrawPanelLabel("PF_sl",     "  Total SL :   0.00 USD",    px+4, 90,  clrTomato,    10);
    DrawPanelLabel("PF_tp",     "  Total TP :   0.00 USD",    px+4, 108, clrLimeGreen, 10);
    DrawPanelLabel("PF_spread", "  Spread   :   0 pts",       px+4, 126, clrLimeGreen, 10);

    // ── Panel 2: Control Buttons (góc phải dưới) ─────────
    int chartH = (int)ChartGetInteger(0, CHART_HEIGHT_IN_PIXELS);
    int p2H    = 102;                     // header 22 + body 80
    int p2Y    = chartH - p2H - 6;      // 10px margin từ cạnh dưới

    DrawPanelRect("PF_P2_hdr",  px, p2Y,      pw, 22, C'20,60,120', C'40,80,160', 2);
    DrawPanelRect("PF_P2_body", px, p2Y + 22, pw, 80, C'10,10,25',  C'40,80,140', 1);

    DrawPanelLabel("PF_P2_ttl", "  Control Buttons", px+4, p2Y + 5, clrWhite, 10);
}

void DeletePanels() {
    string nms[] = {
        "PF_P1_hdr","PF_P1_body","PF_P1_ttl",
        "PF_time","PF_sep1","PF_sl","PF_tp","PF_spread",
        "PF_P2_hdr","PF_P2_body","PF_P2_ttl"
    };
    for(int i = 0; i < ArraySize(nms); i++)
        ObjectDelete(0, nms[i]);
}

void UpdateInfoPanel() {
    int px = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS) - 210;

    MqlDateTime t;
    TimeToStruct(TimeGMT() + InpGMTOffset * 3600, t);
    DrawPanelLabel("PF_time",
        StringFormat("  Time : %04d/%02d/%02d  %02d:%02d:%02d",
            t.year, t.mon, t.day, t.hour, t.min, t.sec),
        px+4, 58, clrSilver, 10);

    double spread    = SymbolInfoDouble(_Symbol, SYMBOL_ASK) - SymbolInfoDouble(_Symbol, SYMBOL_BID);
    int    spreadPts = (int)MathRound(spread / _Point);
    color  spreadClr = (spread / _Point <= 30) ? clrLimeGreen : clrTomato;
    DrawPanelLabel("PF_spread",
        StringFormat("  Spread   : %d pts", spreadPts),
        px+4, 126, spreadClr, 10);
}

void DrawMarkerPrice(ENUM_TIMEFRAMES timeframe, color lineColor) {
    double emaValue[];
    int handle = iMA(_Symbol, timeframe, 25, 0, MODE_EMA, PRICE_CLOSE);
    ;
    if(handle < 0) return;

    ArraySetAsSeries(emaValue, true);
    if(CopyBuffer(handle, 0, 0, 1, emaValue) <= 0) return;

    double price = emaValue[0];
    if(price == 0) return;

    datetime currentTime = iTime(_Symbol, PERIOD_CURRENT, 0);
    datetime start = currentTime + PeriodSeconds(PERIOD_CURRENT) * 10;
    datetime end = currentTime + PeriodSeconds(PERIOD_CURRENT) * 2;

    string lineName = "Price " + DoubleToString(price);
    string textName = "TimeframeLabel_" + IntegerToString(timeframe);

    ObjectCreate(0, lineName, OBJ_TREND, 0, start, price, end, price);
    ObjectSetInteger(0, lineName, OBJPROP_COLOR, lineColor);
    ObjectSetInteger(0, lineName, OBJPROP_WIDTH, 2);

    ObjectCreate(0, textName, OBJ_TEXT, 0, start, price + 52 * _Point);
    ObjectSetString(0, textName, OBJPROP_TEXT,
                    StringSubstr(EnumToString(timeframe), 7));
    ObjectSetInteger(0, textName, OBJPROP_COLOR, lineColor);
    ObjectSetInteger(0, textName, OBJPROP_ANCHOR, ANCHOR_LEFT_UPPER);
    ObjectSetInteger(0, textName, OBJPROP_FONTSIZE, 10);
}

// Vẽ đường EMA liên tục qua nhiều nến dưới dạng chuỗi OBJ_TREND
void DrawEMALine(ENUM_TIMEFRAMES tf, int emaPeriod, color lineColor, int lineWidth, int bars = 80) {
    string prefix = "EMA_" + IntegerToString((int)tf) + "_";

    int handle = iMA(_Symbol, tf, emaPeriod, 0, MODE_EMA, PRICE_CLOSE);
    if(handle == INVALID_HANDLE) return;

    double ema[];
    ArraySetAsSeries(ema, true);
    int copied = CopyBuffer(handle, 0, 0, bars, ema);
    if(copied <= 1) return;

    for(int i = 0; i < copied - 1; i++) {
        datetime t1 = iTime(_Symbol, tf, i + 1);
        datetime t2 = iTime(_Symbol, tf, i);
        if(t1 == 0 || t2 == 0 || ema[i] == 0 || ema[i+1] == 0) continue;

        string nm = prefix + IntegerToString(i);
        if(ObjectFind(0, nm) < 0)
            ObjectCreate(0, nm, OBJ_TREND, 0, t1, ema[i+1], t2, ema[i]);
        else {
            ObjectMove(0, nm, 0, t1, ema[i+1]);
            ObjectMove(0, nm, 1, t2, ema[i]);
        }
        ObjectSetInteger(0, nm, OBJPROP_COLOR,      lineColor);
        ObjectSetInteger(0, nm, OBJPROP_WIDTH,      lineWidth);
        ObjectSetInteger(0, nm, OBJPROP_STYLE,      STYLE_SOLID);
        ObjectSetInteger(0, nm, OBJPROP_RAY_RIGHT,  false);
        ObjectSetInteger(0, nm, OBJPROP_RAY_LEFT,   false);
        ObjectSetInteger(0, nm, OBJPROP_SELECTABLE, false);
        ObjectSetInteger(0, nm, OBJPROP_BACK,       true);
    }
}

void DeleteEMALines() {
    ObjectsDeleteAll(0, "EMA_", -1, OBJ_TREND);
}

void Draw() {
    ObjectsDeleteAll(0, "Price ", -1, OBJ_TREND);
    ObjectsDeleteAll(0, "TimeframeLabel_", -1, OBJ_TEXT);
    DeleteEMALines();

    // Marker H1 và M15 luôn vẽ
    DrawMarkerPrice(PERIOD_H1,  clrGray);
    DrawMarkerPrice(PERIOD_M15, clrTeal);

    // Số nến đang hiển thị trên chart
    int vis = (int)ChartGetInteger(0, CHART_VISIBLE_BARS);

    // EMA thay đổi theo khung hiện tại, vẽ đủ toàn bộ nến hiển thị
    if(_Period == PERIOD_M1) {
        int barsM5 = vis * PeriodSeconds(PERIOD_M1) / PeriodSeconds(PERIOD_M5) + 5;
        DrawEMALine(PERIOD_M1, 25, clrLime,       1, vis);
        DrawEMALine(PERIOD_M5, 25, clrDodgerBlue, 2, barsM5);
    } else if(_Period == PERIOD_M5) {
        int barsM15 = vis * PeriodSeconds(PERIOD_M5) / PeriodSeconds(PERIOD_M15) + 5;
        DrawEMALine(PERIOD_M5,  25, clrDodgerBlue, 1, vis);
        DrawEMALine(PERIOD_M15, 25, clrOrange,     2, barsM15);
    } else if(_Period == PERIOD_M15) {
        int barsH1 = vis * PeriodSeconds(PERIOD_M15) / PeriodSeconds(PERIOD_H1) + 5;
        DrawEMALine(PERIOD_M15, 25, clrOrange, 1, vis);
        DrawEMALine(PERIOD_H1,  25, clrGray,   2, barsH1);
    } else if(_Period == PERIOD_H1) {
        DrawEMALine(PERIOD_H1, 25, clrGray, 2, vis);
    }
}

void CalculateForLable() {
    double totalSl = 0, totalTp = 0;

    double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
    double tickSize = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
    double pointValue = tickValue / tickSize;

    for(int index = PositionsTotal() - 1; index >= 0; index--) {
        ulong ticket = PositionGetTicket(index);

        if(PositionSelectByTicket(ticket)) {
            double sl = PositionGetDouble(POSITION_SL);
            double tp = PositionGetDouble(POSITION_TP);

            double price = PositionGetDouble(POSITION_PRICE_OPEN);
            double volume = PositionGetDouble(POSITION_VOLUME);
            ENUM_POSITION_TYPE type =
                (ENUM_POSITION_TYPE)PositionGetInteger(POSITION_TYPE);

            if(sl != 0) {
                double diff = 0;

                if(type == POSITION_TYPE_BUY) {
                    diff = sl - price;  // BUY: SL > Price = lỗ
                } else
                    diff = price - sl;  // SELL: SL < Price = lỗ

                totalSl += diff * volume * pointValue * 100;
            }

            if(tp != 0) {
                double diff = 0;

                if(type == POSITION_TYPE_BUY) {
                    diff = tp - price;  // BUY: TP > Price = lời
                } else
                    diff = price - tp;  // SELL: TP < Price = lời

                totalTp += diff * volume * pointValue * 100;
            }
        }
    }

    ObjectSetString (0, "PF_sl", OBJPROP_TEXT,
        StringFormat("  Total SL :  %.2f USD", totalSl));
    ObjectSetInteger(0, "PF_sl", OBJPROP_COLOR,
        (totalSl >= 0 ? clrLimeGreen : clrTomato));

    ObjectSetString (0, "PF_tp", OBJPROP_TEXT,
        StringFormat("  Total TP :  %.2f USD", totalTp));
    ObjectSetInteger(0, "PF_tp", OBJPROP_COLOR,
        (totalTp >= 0 ? clrLimeGreen : clrTomato));
}

void ModifyStopLoss(ulong ticket, double newStopLoss, double currentTP) {
    newStopLoss = NormalizeDouble(newStopLoss, _Digits);
    if(!Trade.PositionModify(ticket, newStopLoss, currentTP)) {
        Print("Failed to modify position #", ticket,
              ". Error: ", GetLastError());
    }
}

void CheckAndAdjustStopLoss() {
    // Duyệt qua tất cả các lệnh đang mở
    for(int index = PositionsTotal() - 1; index >= 0; index--) {
        ulong ticket = PositionGetTicket(index);

        if(PositionSelectByTicket(ticket)) {
            double lot = PositionGetDouble(POSITION_VOLUME);
            long type = PositionGetInteger(POSITION_TYPE);
            double priceOpen = PositionGetDouble(POSITION_PRICE_OPEN);
            double currentSL = PositionGetDouble(POSITION_SL);
            double curentTP = PositionGetDouble(POSITION_TP);

            double tickValue =
                SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
            double tickSize = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);

            if(tickValue <= 0) continue;  // Tránh lỗi chia cho 0

            double slDistance =
                (InpMaxLossAmount / (lot * tickValue * 100)) * tickSize;
            double targetSL = 0;

            if(type == POSITION_TYPE_BUY) {
                targetSL = NormalizeDouble(priceOpen - slDistance, _Digits);

                if(currentSL == 0 || currentSL < targetSL) {
                    ModifyStopLoss(ticket, targetSL, curentTP);
                }
            } else if(type == POSITION_TYPE_SELL) {
                targetSL = NormalizeDouble(priceOpen + slDistance, _Digits);

                if(currentSL == 0 || currentSL > targetSL) {
                    ModifyStopLoss(ticket, targetSL, curentTP);
                }
            }
        }
    }
}

void CheckPriceWithEMA() {
    double emaM1[], emaM5[];

    int handleM1 = iMA(_Symbol, PERIOD_M1, 25, 0, MODE_EMA, PRICE_CLOSE);
    int handleM5 = iMA(_Symbol, PERIOD_M5, 25, 0, MODE_EMA, PRICE_CLOSE);
    if(handleM1 < 0 || handleM5 < 0) return;

    ArraySetAsSeries(emaM1, true);
    ArraySetAsSeries(emaM5, true);

    if(CopyBuffer(handleM1, 0, 0, 1, emaM1) <= 0) return;
    if(CopyBuffer(handleM5, 0, 0, 1, emaM5) <= 0) return;

    double high = iHigh(_Symbol, PERIOD_CURRENT, 0),
           low = iLow(_Symbol, PERIOD_CURRENT, 0);

    double emaPriceM1 = emaM1[0];
    double emaPriceM5 = emaM5[0];

    bool alertChecked = false;
    if(high > emaPriceM1 && low < emaPriceM1) {
        Alert("Giá đã chạm EMA25 M1");
        alertChecked = true;
    } else if(high > emaPriceM5 && low < emaPriceM5) {
        Alert("Giá đã chạm EMA25 M5");
        alertChecked = true;
    }

    if(alertChecked) {
        AlertCheckEnabled = !AlertCheckEnabled;

        if(AlertCheckEnabled) {
            btnAlertCheck.Description("+ CHECK EMA: ON");
            btnAlertCheck.BackColor(C'20,60,150');
            btnAlertCheck.BorderColor(C'80,130,230');
        } else {
            btnAlertCheck.Description("X CHECK EMA: OFF");
            btnAlertCheck.BackColor(C'145,15,15');
            btnAlertCheck.BorderColor(C'230,65,65');
        }
    }
}

// Xác định nến NOsd: nến hiện tại là SELL, nến trước là BUY, high hiện tại > high trước
void CheckNOsdCandle() {
    double currOpen  = iOpen (_Symbol, PERIOD_CURRENT, 0);
    double currClose = iClose(_Symbol, PERIOD_CURRENT, 0);
    double currHigh  = iHigh (_Symbol, PERIOD_CURRENT, 0);
    double currLow   = iLow  (_Symbol, PERIOD_CURRENT, 0);

    double prevOpen  = iOpen (_Symbol, PERIOD_CURRENT, 1);
    double prevClose = iClose(_Symbol, PERIOD_CURRENT, 1);
    double prevHigh  = iHigh (_Symbol, PERIOD_CURRENT, 1);

    bool currIsSell = currClose < currOpen;
    bool prevIsBuy  = prevClose > prevOpen;
    bool sweepsHigh = currHigh > prevHigh;

    datetime candleTime = iTime(_Symbol, PERIOD_CURRENT, 0);
    datetime candleEnd  = candleTime + PeriodSeconds(PERIOD_CURRENT) - 1;
    string   name       = "NOsd_" + TimeToString(candleTime, TIME_DATE | TIME_MINUTES);

    if(currIsSell && prevIsBuy && sweepsHigh) {
        if(ObjectFind(0, name) < 0) {
            ObjectCreate(0, name, OBJ_RECTANGLE, 0, candleTime, currHigh, candleEnd, currLow);
            ObjectSetInteger(0, name, OBJPROP_COLOR,      clrLime);
            ObjectSetInteger(0, name, OBJPROP_STYLE,      STYLE_SOLID);
            ObjectSetInteger(0, name, OBJPROP_WIDTH,      1);
            ObjectSetInteger(0, name, OBJPROP_FILL,       true);
            ObjectSetInteger(0, name, OBJPROP_BACK,       true);
            ObjectSetInteger(0, name, OBJPROP_SELECTABLE, false);
        } else {
            // Cập nhật khi nến đang hình thành
            ObjectMove(0, name, 0, candleTime, currHigh);
            ObjectMove(0, name, 1, candleEnd,  currLow);
        }
    } else {
        if(ObjectFind(0, name) >= 0)
            ObjectDelete(0, name);
    }

    ChartRedraw(0);
}

// ══════════════════════════════════════════════════════════
//  ORDER PANEL
// ══════════════════════════════════════════════════════════

void CreateEditBox(string nm, int x, int y, int w, int h, string txt) {
    if(ObjectFind(0, nm) < 0) ObjectCreate(0, nm, OBJ_EDIT, 0, 0, 0);
    ObjectSetInteger(0, nm, OBJPROP_CORNER,       CORNER_LEFT_UPPER);
    ObjectSetInteger(0, nm, OBJPROP_XDISTANCE,    x);
    ObjectSetInteger(0, nm, OBJPROP_YDISTANCE,    y);
    ObjectSetInteger(0, nm, OBJPROP_XSIZE,        w);
    ObjectSetInteger(0, nm, OBJPROP_YSIZE,        h);
    ObjectSetString (0, nm, OBJPROP_TEXT,         txt);
    ObjectSetString (0, nm, OBJPROP_FONT,         "Consolas");
    ObjectSetInteger(0, nm, OBJPROP_FONTSIZE,     9);
    ObjectSetInteger(0, nm, OBJPROP_COLOR,        clrWhite);
    ObjectSetInteger(0, nm, OBJPROP_BGCOLOR,      C'15,15,35');
    ObjectSetInteger(0, nm, OBJPROP_BORDER_COLOR, C'50,80,140');
    ObjectSetInteger(0, nm, OBJPROP_ALIGN,        ALIGN_LEFT);
    ObjectSetInteger(0, nm, OBJPROP_SELECTABLE,   false);
    ObjectSetInteger(0, nm, OBJPROP_HIDDEN,       true);
}

void CreateOrderButton(string nm, string txt, int x, int y, int w, color bg, color border) {
    if(ObjectFind(0, nm) < 0) ObjectCreate(0, nm, OBJ_BUTTON, 0, 0, 0);
    ObjectSetInteger(0, nm, OBJPROP_CORNER,       CORNER_LEFT_UPPER);
    ObjectSetInteger(0, nm, OBJPROP_XDISTANCE,    x);
    ObjectSetInteger(0, nm, OBJPROP_YDISTANCE,    y);
    ObjectSetInteger(0, nm, OBJPROP_XSIZE,        w);
    ObjectSetInteger(0, nm, OBJPROP_YSIZE,        28);
    ObjectSetString (0, nm, OBJPROP_TEXT,         txt);
    ObjectSetString (0, nm, OBJPROP_FONT,         "Consolas");
    ObjectSetInteger(0, nm, OBJPROP_FONTSIZE,     9);
    ObjectSetInteger(0, nm, OBJPROP_COLOR,        clrWhite);
    ObjectSetInteger(0, nm, OBJPROP_BGCOLOR,      bg);
    ObjectSetInteger(0, nm, OBJPROP_BORDER_COLOR, border);
    ObjectSetInteger(0, nm, OBJPROP_SELECTABLE,   false);
    ObjectSetInteger(0, nm, OBJPROP_HIDDEN,       true);
}

void CreateOrderPanel() {
    int px = 10, py = 30, pw = 310, bw = 148;
    int bx1 = px+4, bx2 = bx1+bw+6;
    int by  = py + 22;

    DrawPanelRect("OP_hdr",  px, py,      pw, 22,  C'20,60,120', C'40,80,160', 2);
    DrawPanelRect("OP_body", px, py + 22, pw, 210, C'10,10,25',  C'40,80,140', 1);
    DrawPanelLabel("OP_ttl", "  ORDER SETUP", px+4, py+5, clrWhite, 10);

    // Lots
    DrawPanelLabel("OP_LbLots", "  Lots", bx1, by+6, clrSilver, 9);
    CreateEditBox("OP_EditLots", bx1, by+18, bw, 22, DoubleToString(InpDefaultLots, 2));

    // TP & SL
    DrawPanelLabel("OP_LbTP", "  TP (pts)", bx1, by+50, clrSilver, 9);
    DrawPanelLabel("OP_LbSL", "  SL (pts)", bx2, by+50, clrSilver, 9);
    CreateEditBox("OP_EditTP", bx1, by+62, bw, 22, IntegerToString(InpDefaultTP));
    CreateEditBox("OP_EditSL", bx2, by+62, bw, 22, IntegerToString(InpDefaultSL));

    // Comment
    DrawPanelLabel("OP_LbCmt", "  Comment", bx1, by+94, clrSilver, 9);
    CreateEditBox("OP_EditCmt", bx1, by+106, pw-10, 22, "");

    // Market order buttons only
    CreateOrderButton("OP_BtnSell",   "▼  SELL",   bx1, by+138, bw, C'180,20,20', C'230,65,65');
    CreateOrderButton("OP_BtnBuy",    "▲  BUY",    bx2, by+138, bw, C'0,130,60',  C'45,185,90');
    CreateOrderButton("OP_BtnCancel", "X  CANCEL", bx1, by+174, bw, C'55,55,55',  C'110,110,110');
    CreateOrderButton("OP_BtnSend",   ">> SEND",   bx2, by+174, bw, C'20,60,150', C'80,130,230');
}

void DeleteOrderPanel() {
    string nms[] = {
        "OP_hdr","OP_body","OP_ttl",
        "OP_LbLots","OP_EditLots",
        "OP_LbTP","OP_LbSL","OP_EditTP","OP_EditSL",
        "OP_LbCmt","OP_EditCmt",
        "OP_BtnSell","OP_BtnBuy",
        "OP_BtnCancel","OP_BtnSend"
    };
    for(int i = 0; i < ArraySize(nms); i++)
        ObjectDelete(0, nms[i]);
}

void ExecuteOrderFromPanel(string otype) {
    if(g_pendingLots <= 0) { Alert("Lots phai > 0"); return; }

    double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);

    g_lastOrderType = otype;

    if(otype == "BUY")
        Trade.Buy(g_pendingLots, _Symbol, ask, g_pendingSLPrice, g_pendingTPPrice, g_pendingCmt);
    else if(otype == "SELL")
        Trade.Sell(g_pendingLots, _Symbol, bid, g_pendingSLPrice, g_pendingTPPrice, g_pendingCmt);
}

void PrepareOrder(string otype) {
    double lots  = StringToDouble(ObjectGetString(0, "OP_EditLots", OBJPROP_TEXT));
    double tpPts = StringToDouble(ObjectGetString(0, "OP_EditTP",   OBJPROP_TEXT));
    double slPts = StringToDouble(ObjectGetString(0, "OP_EditSL",   OBJPROP_TEXT));
    string cmt   = ObjectGetString(0, "OP_EditCmt", OBJPROP_TEXT);

    if(lots <= 0) { Alert("Lots phai > 0"); return; }

    double close = iClose(_Symbol, PERIOD_CURRENT, 0);

    g_pendingOrderType  = otype;
    g_pendingLots       = lots;
    g_pendingCmt        = cmt;
    g_pendingTPPts      = tpPts;
    g_pendingSLPts      = slPts;
    g_pendingEntryPrice = close;

    if(otype == "BUY") {
        g_pendingTPPrice = (tpPts > 0) ? NormalizeDouble(close + tpPts * _Point, _Digits) : 0;
        g_pendingSLPrice = (slPts > 0) ? NormalizeDouble(close - slPts * _Point, _Digits) : 0;
    } else {
        g_pendingTPPrice = (tpPts > 0) ? NormalizeDouble(close - tpPts * _Point, _Digits) : 0;
        g_pendingSLPrice = (slPts > 0) ? NormalizeDouble(close + slPts * _Point, _Digits) : 0;
    }

    ShowOrderPreview(true);
}

void CreateHLine(string nm, double price, color clr, int width, ENUM_LINE_STYLE style) {
    if(ObjectFind(0, nm) < 0) ObjectCreate(0, nm, OBJ_HLINE, 0, 0, price);
    ObjectSetDouble (0, nm, OBJPROP_PRICE,      price);
    ObjectSetInteger(0, nm, OBJPROP_COLOR,      clr);
    ObjectSetInteger(0, nm, OBJPROP_WIDTH,      width);
    ObjectSetInteger(0, nm, OBJPROP_STYLE,      style);
    ObjectSetInteger(0, nm, OBJPROP_SELECTABLE, false);
    ObjectSetInteger(0, nm, OBJPROP_BACK,       true);
}

void CreateZoneRect(string nm, double priceLo, double priceHi, color clr, datetime t1, datetime t2) {
    if(ObjectFind(0, nm) < 0) ObjectCreate(0, nm, OBJ_RECTANGLE, 0, t1, priceHi, t2, priceLo);
    else {
        ObjectMove(0, nm, 0, t1, priceHi);
        ObjectMove(0, nm, 1, t2, priceLo);
    }
    ObjectSetInteger(0, nm, OBJPROP_COLOR,      clr);
    ObjectSetInteger(0, nm, OBJPROP_FILL,       true);
    ObjectSetInteger(0, nm, OBJPROP_BACK,       true);
    ObjectSetInteger(0, nm, OBJPROP_STYLE,      STYLE_SOLID);
    ObjectSetInteger(0, nm, OBJPROP_WIDTH,      1);
    ObjectSetInteger(0, nm, OBJPROP_SELECTABLE, false);
}

void CreateTextObj(string nm, string txt, datetime t, double price, color clr,
                   ENUM_ANCHOR_POINT anchor = ANCHOR_LEFT_UPPER) {
    if(ObjectFind(0, nm) < 0) ObjectCreate(0, nm, OBJ_TEXT, 0, t, price);
    ObjectMove(0, nm, 0, t, price);
    ObjectSetString (0, nm, OBJPROP_TEXT,       txt);
    ObjectSetInteger(0, nm, OBJPROP_COLOR,      clr);
    ObjectSetInteger(0, nm, OBJPROP_FONTSIZE,   9);
    ObjectSetString (0, nm, OBJPROP_FONT,       "Consolas");
    ObjectSetInteger(0, nm, OBJPROP_ANCHOR,     anchor);
    ObjectSetInteger(0, nm, OBJPROP_SELECTABLE, false);
    ObjectSetInteger(0, nm, OBJPROP_BACK,       false);
}

void ShowOrderPreview(bool deleteOld = true) {
    // Đọc globals vào local TRƯỚC KHI xóa object cũ
    double entry = g_pendingEntryPrice;
    double tp    = g_pendingTPPrice;
    double sl    = g_pendingSLPrice;
    bool   isBuy = (g_pendingOrderType == "BUY");

    if(deleteOld) {
        string oldObjs[] = {"OPV_EntryStrip","OPV_TPStrip","OPV_SLStrip",
                            "OPV_Entry","OPV_TP","OPV_SL",
                            "OPV_ZoneTP","OPV_ZoneSL",
                            "OPV_TxtEntry","OPV_TxtTP","OPV_TxtSL"};
        for(int i = 0; i < ArraySize(oldObjs); i++)
            ObjectDelete(0, oldObjs[i]);
    }

    // Tính toán USD và %
    double tickValue  = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
    double tickSize   = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
    double balance    = AccountInfoDouble(ACCOUNT_BALANCE);
    double slUSD = 0, tpUSD = 0, slPct = 0, tpPct = 0;
    if(tickSize > 0) {
        if(sl > 0) { slUSD = MathAbs(entry - sl) / tickSize * tickValue * g_pendingLots; }
        if(tp > 0) { tpUSD = MathAbs(tp - entry) / tickSize * tickValue * g_pendingLots; }
    }
    if(balance > 0) { slPct = slUSD / balance * 100; tpPct = tpUSD / balance * 100; }

    color tpZoneClr = C'100,220,150';
    color slZoneClr = C'230,120,120';
    color tpClr     = C'0,210,90';
    color slClr     = C'220,50,50';

    datetime zoneT1  = iTime(_Symbol, PERIOD_CURRENT, 17);
    datetime zoneT2  = iTime(_Symbol, PERIOD_CURRENT, 3);

    // Trim strip endpoints để bù trừ line-cap của OBJ_TREND (11px wide → ~5.5px mỗi đầu)
    int      cw    = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS);
    int      vbars = (int)ChartGetInteger(0, CHART_VISIBLE_BARS);
    double   ppb   = (vbars > 0 && cw > 0) ? (double)cw / vbars : 6.0;
    int      trimB = MathMax(1, (int)MathCeil(6.0 / ppb));
    datetime pad   = (datetime)(trimB * PeriodSeconds(PERIOD_CURRENT));
    datetime sT1   = zoneT1 + pad;
    datetime sT2   = (zoneT2 > sT1 + pad) ? zoneT2 - pad : zoneT2;

    datetime lblTime = zoneT1;
    string   side    = isBuy ? "Buy" : "Sell";

    // Entry: HLINE nét đứt + dải trắng + chữ đen
    CreateHLine("OPV_Entry", entry, C'180,180,180', 1, STYLE_DASH);
    string stripNm = "OPV_EntryStrip";
    if(ObjectFind(0, stripNm) < 0) ObjectCreate(0, stripNm, OBJ_TREND, 0, sT1, entry, sT2, entry);
    else { ObjectMove(0, stripNm, 0, sT1, entry); ObjectMove(0, stripNm, 1, sT2, entry); }
    ObjectSetInteger(0, stripNm, OBJPROP_COLOR,     C'235,235,235');
    ObjectSetInteger(0, stripNm, OBJPROP_WIDTH,     11);
    ObjectSetInteger(0, stripNm, OBJPROP_STYLE,     STYLE_SOLID);
    ObjectSetInteger(0, stripNm, OBJPROP_RAY_RIGHT, false);
    ObjectSetInteger(0, stripNm, OBJPROP_RAY_LEFT,  false);
    ObjectSetInteger(0, stripNm, OBJPROP_SELECTABLE,false);
    ObjectSetInteger(0, stripNm, OBJPROP_BACK,      false);
    CreateTextObj("OPV_TxtEntry",
        StringFormat("%s|%s|Lots %.2f", side, DoubleToString(entry, _Digits), g_pendingLots),
        lblTime, entry, clrBlack, ANCHOR_LEFT);
    ObjectSetInteger(0, "OPV_TxtEntry", OBJPROP_ZORDER, 1);

    // TP: HLINE toàn chart + white strip + zone + chữ đen căn giữa strip
    if(tp > 0) {
        CreateHLine("OPV_TP", tp, tpClr, 1, STYLE_SOLID);
        string tpStrip = "OPV_TPStrip";
        if(ObjectFind(0, tpStrip) < 0) ObjectCreate(0, tpStrip, OBJ_TREND, 0, sT1, tp, sT2, tp);
        else { ObjectMove(0, tpStrip, 0, sT1, tp); ObjectMove(0, tpStrip, 1, sT2, tp); }
        ObjectSetInteger(0, tpStrip, OBJPROP_COLOR,      C'235,235,235');
        ObjectSetInteger(0, tpStrip, OBJPROP_WIDTH,      11);
        ObjectSetInteger(0, tpStrip, OBJPROP_STYLE,      STYLE_SOLID);
        ObjectSetInteger(0, tpStrip, OBJPROP_RAY_RIGHT,  false);
        ObjectSetInteger(0, tpStrip, OBJPROP_RAY_LEFT,   false);
        ObjectSetInteger(0, tpStrip, OBJPROP_SELECTABLE, false);
        ObjectSetInteger(0, tpStrip, OBJPROP_BACK,       false);
        if(isBuy) {
            CreateZoneRect("OPV_ZoneTP", entry, tp, tpZoneClr, zoneT1, zoneT2);
        } else {
            CreateZoneRect("OPV_ZoneTP", tp, entry, tpZoneClr, zoneT1, zoneT2);
        }
        CreateTextObj("OPV_TxtTP",
            StringFormat("TP %s|%.2f USD|%.2f%%", DoubleToString(tp, _Digits), tpUSD, tpPct),
            lblTime, tp, clrBlack, ANCHOR_LEFT);
        ObjectSetInteger(0, "OPV_TxtTP", OBJPROP_ZORDER, 1);
    }

    // SL: HLINE toàn chart + white strip + zone + chữ đen căn giữa strip
    if(sl > 0) {
        CreateHLine("OPV_SL", sl, slClr, 1, STYLE_SOLID);
        string slStrip = "OPV_SLStrip";
        if(ObjectFind(0, slStrip) < 0) ObjectCreate(0, slStrip, OBJ_TREND, 0, sT1, sl, sT2, sl);
        else { ObjectMove(0, slStrip, 0, sT1, sl); ObjectMove(0, slStrip, 1, sT2, sl); }
        ObjectSetInteger(0, slStrip, OBJPROP_COLOR,      C'235,235,235');
        ObjectSetInteger(0, slStrip, OBJPROP_WIDTH,      11);
        ObjectSetInteger(0, slStrip, OBJPROP_STYLE,      STYLE_SOLID);
        ObjectSetInteger(0, slStrip, OBJPROP_RAY_RIGHT,  false);
        ObjectSetInteger(0, slStrip, OBJPROP_RAY_LEFT,   false);
        ObjectSetInteger(0, slStrip, OBJPROP_SELECTABLE, false);
        ObjectSetInteger(0, slStrip, OBJPROP_BACK,       false);
        if(isBuy) {
            CreateZoneRect("OPV_ZoneSL", sl, entry, slZoneClr, zoneT1, zoneT2);
        } else {
            CreateZoneRect("OPV_ZoneSL", entry, sl, slZoneClr, zoneT1, zoneT2);
        }
        CreateTextObj("OPV_TxtSL",
            StringFormat("SL %s|%.2f USD|%.2f%%", DoubleToString(sl, _Digits), slUSD, slPct),
            lblTime, sl, clrBlack, ANCHOR_LEFT);
        ObjectSetInteger(0, "OPV_TxtSL", OBJPROP_ZORDER, 1);
    }

    ChartRedraw(0);
}

void UpdateOrderPreview() {
    if(g_pendingOrderType == "") return;
    double newEntry = iClose(_Symbol, PERIOD_CURRENT, 0);
    if(MathAbs(newEntry - g_pendingEntryPrice) < _Point) return;
    g_pendingEntryPrice = newEntry;
    ShowOrderPreview(false);
}

void ClearOrderPreview() {
    string objs[] = {
        "OPV_EntryStrip","OPV_TPStrip","OPV_SLStrip",
        "OPV_Entry","OPV_TP","OPV_SL",
        "OPV_ZoneTP","OPV_ZoneSL",
        "OPV_TxtEntry","OPV_TxtTP","OPV_TxtSL"
    };
    for(int i = 0; i < ArraySize(objs); i++)
        ObjectDelete(0, objs[i]);

    g_pendingOrderType  = "";
    g_pendingEntryPrice = 0;
    g_pendingTPPrice    = 0;
    g_pendingSLPrice    = 0;
    g_pendingTPPts      = 0;
    g_pendingSLPts      = 0;
    g_pendingLots       = 0;
    g_pendingCmt        = "";
    ChartRedraw(0);
}
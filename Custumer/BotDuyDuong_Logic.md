# BB Breakout Bot — Duy Duong v2.0
**File:** `BotDuyDuong.mq5` | **Ngày:** 30.05.2026

---

## Tổng quan

Bot giao dịch theo mô hình **Bollinger Bands Breakout**: giá quét qua band BB → hồi lại → xác nhận cấu trúc → vào lệnh pending stop. Lọc tín hiệu bằng **Stochastic** và **Supertrend**. Chỉ có tối đa 1 setup active tại 1 thời điểm (BUY hoặc SELL, không cùng lúc).

---

## Chỉ báo sử dụng

| Chỉ báo | Tham số mặc định | Mục đích |
|---|---|---|
| Bollinger Bands | Period=20, Dev=2.0 | Xác định trigger và đường giữa |
| Stochastic | K=14, D=3, Slow=1 | Lọc trạng thái quá mua/quá bán |
| ATR (cho SuperTrend) | Period=10, Mult=1.2 | Lọc xu hướng |

---

## State Machine

Mỗi hướng (BUY/SELL) có 3 trạng thái:

```
checkBuy / checkSell:
  0 = IDLE          (không có setup nào)
  1 = TRACKING      (đang theo dõi setup)
  2 = ORDER PLACED  (lệnh pending đã đặt)

checkFirstbuy / checkFirstsell:
  0 = Chờ xác nhận đỉnh/đáy
  1 = Đỉnh/đáy đã được xác nhận
```

---

## Logic BUY (chi tiết)

### Bước 1 — Trigger (kích hoạt)
**Điều kiện:** Nến đã đóng (bar[1]) có:
- `low[1] < Lower BB[1]` (wick/body quét dưới band dưới)
- `close[1] < middle[1]` (đóng cửa dưới đường giữa)
- Không có setup BUY/SELL nào đang active

**Kết quả:**
- `checkBuy = 1`, `checkFirstbuy = 0`
- Ghi nhận `buyMinLow = low[1]` (đáy tham chiếu)
- Ghi nhận `buyTriggerCandleHigh = high[1]` (high cần phá vỡ để xác nhận đỉnh)
- Vẽ marker **xanh ▲** dưới nến trigger

---

### Bước 2 — Xác nhận đỉnh (`checkFirstbuy = 0 → 1`)
**Điều kiện:** Nến đã đóng (bar[1]) thỏa:
- `close[1] > open[1]` (nến tăng)
- `high[1] > buyTriggerCandleHigh` (phá lên trên high nến trigger)
- `close[1] <= middle[1]` (đóng cửa DƯỚI đường giữa BB)
- `low[1] >= buyMinLow` (đáy không bị thủng — đảm bảo bởi cấu trúc code)

**Kết quả:**
- `checkFirstbuy = 1`
- Ghi nhận `buyPeakHigh = high[1]`
- Ghi nhận `buyBullishPhaseLow = low[1]` (đáy thấp nhất của phase tăng)
- Vẽ marker **cam ▼** trên đỉnh nến xác nhận

**Bác bỏ (reset hoàn toàn):** Nếu nến tăng nhưng `close[1] > middle[1]` → giá đã vượt qua đường giữa, setup vô hiệu.

---

### Bước 3 — Vào lệnh BuyStop (`checkFirstbuy = 1 → checkBuy = 2`)
**Điều kiện:** bar[2] là nến tăng + bar[1] là nến giảm (pattern xác nhận đỉnh đảo chiều):
- `close[2] > open[2]` (bar[2] tăng — nến đỉnh)
- `close[1] < open[1]` (bar[1] giảm — nến xác nhận vào)
- `close[1] < buyBullishPhaseLow` (giá pullback xuống dưới đáy phase tăng)
- `close[2] <= middle[2]` (đỉnh xác nhận vẫn dưới đường giữa)
- `buyPeakHigh > 0`

**Bộ lọc:**
- **Stochastic:** %D phải đã chạm OversoldLevel (≤30) và chưa vượt MidLevel (50) trong khoảng từ trigger đến hiện tại
- **Supertrend:** Đỉnh BUY (`buyPeakHigh`) phải nằm dưới Upper Band Supertrend

**Vào lệnh:**
- `Entry = MathMax(buyPeakHigh, high[2]) + Spread` (BuyStop trên đỉnh cao nhất)
- `SL = buyMinLow` (đặt sau khi lệnh kích hoạt qua `UpdateStopLossFromHistory`)
- `Lot` tính theo % rủi ro tài khoản

---

## Logic SELL (đối xứng với BUY)

### Bước 1 — Trigger
- `high[1] > Upper BB[1]` và `close[1] > middle[1]`
- Ghi nhận `sellMaxHigh`, `sellTriggerCandleLow`
- Vẽ marker **đỏ ▼** trên nến trigger

### Bước 2 — Xác nhận đáy (`checkFirstsell = 0 → 1`)
- Nến giảm (`close[1] < open[1]`) phá xuống dưới `sellTriggerCandleLow`
- `close[1] >= middle[1]` (đóng cửa TRÊN đường giữa)
- Vẽ marker **xanh dương ▲** dưới đáy xác nhận

### Bước 3 — Vào lệnh SellStop
- bar[2] giảm + bar[1] tăng (pullback từ đáy)
- `close[1] > sellBearishPhaseHigh` (giá hồi vượt đỉnh phase giảm)
- `Entry = MathMin(sellBottomLow, low[2])` (SellStop dưới đáy thấp nhất)
- `SL = sellMaxHigh + Spread`

---

## Các điều kiện hủy setup

### BUY — Reset hoàn toàn về IDLE
| Tình huống | Điều kiện |
|---|---|
| Đáy đầu tiên bị thủng (nến đã đóng) | `low[1] < buyMinLow` (bất kể checkFirstbuy) |
| Đỉnh đã xác nhận nhưng giá vượt đường giữa | `checkFirstbuy==1` và `close[1] >= middle[1]` |
| Nến tăng xác nhận đỉnh nhưng close > middle | `close[1] > middle[1]` khi đang xác nhận |
| Giá vượt đường giữa trong khi giá hiện tại (bar[0]) phá đáy | `low[0] < buyMinLow` và `buyTouchedMiddle==true` |

### BUY — Restart trigger với nến hiện tại (bar[0])
| Tình huống | Điều kiện |
|---|---|
| Nến đang hình thành phá đáy, chưa chạm đường giữa | `low[0] < buyMinLow` và `buyTouchedMiddle==false` |

> **Lưu ý quan trọng:** Nếu đáy đầu tiên bị thủng bởi **nến đã đóng (bar[1])**, setup luôn reset hoàn toàn dù chưa hay đã xác nhận đỉnh — đây là tín hiệu setup false. Trigger mới chỉ được tạo nếu nến mới cũng phá Lower BB (thông qua TRIGGER BUY block cuối tick).

### SELL — Tương tự đối xứng
- `high[1] > sellMaxHigh` → reset hoàn toàn
- `checkFirstsell==1` và `close[1] <= middle[1]` → reset
- `high[0] > sellMaxHigh` + không chạm middle → restart trigger bar[0]

---

## Quản lý lệnh sau khi đặt

### Hủy lệnh pending
- BUY: Nếu `low[0] < buyMinLow` → xóa BuyStop
- SELL: Nếu `high[0] > sellMaxHigh` → xóa SellStop

### SL/TP sau khi lệnh kích hoạt
Áp dụng qua `UpdateStopLossFromHistory` (chạy mỗi tick khi chưa có SL):
- **BUY:** `SL = buyMinLow`, `TP = entry + risk × TakeProfit`
- **SELL:** `SL = sellMaxHigh + spread`, `TP = entry - risk × TakeProfit`

### Đóng 50% tại TP1
- `TP1Price = entry ± risk × TakeProfit1`
- Đóng 50% lot khi giá chạm TP1
- Phần còn lại chạy đến TP2

### Reset sau lệnh
- **Thắng (TP hit hoặc profit > 0):** Reset toàn bộ state, xóa marks
- **Thua (SL hit):** Reset state nhưng **giữ nguyên marks** trên chart để tham khảo

---

## Quản lý vốn

```
risk = balance × RiskPercent / 100
SL_distance = |entry - SL|
lot = risk / (SL_distance / tickSize × tickValue)
```
Lot được chuẩn hóa theo bước lot và giới hạn min/max của broker.

---

## Bộ lọc phiên giao dịch

- Hỗ trợ **2 phiên** cấu hình riêng (Start/End time)
- **MaxTradesInWindow**: giới hạn số lệnh mỗi phiên
- Ngoài phiên: tự động xóa lệnh pending

---

## Bộ lọc tin tức (MT5 Calendar)

- Tự động lấy tin tức quan trọng từ MT5 Economic Calendar
- Dừng giao dịch `InpNewsPauseBefore` phút trước tin
- Mở lại `InpNewsResumeAfter` phút sau tin
- Lọc theo danh sách tiền tệ: mặc định `USD,EUR,GBP,JPY,XAU`

---

## Markers trên Chart

| Marker | Ký hiệu | Màu | Ý nghĩa |
|---|---|---|---|
| `MK_BT{time}` | ▲ (lớn) | Xanh lá | BUY trigger — đáy đầu tiên |
| `MK_BPK{time}` | ▼ (nhỏ) | Cam | BUY peak — đỉnh xác nhận |
| `MK_ST{time}` | ▼ (lớn) | Đỏ | SELL trigger — đỉnh đầu tiên |
| `MK_SBT{time}` | ▲ (lớn) | Xanh dương | SELL bottom — đáy xác nhận |

---

## Sơ đồ trạng thái BUY

```
IDLE (checkBuy=0)
    │
    │ low[1] < LowerBB  AND  close[1] < middle
    ▼
TRACKING (checkBuy=1, checkFirstbuy=0)
    │  "Chờ nến tăng"
    │
    ├─ low[1] < buyMinLow ──────────────────────────► IDLE (reset)
    │
    ├─ low[0] < buyMinLow (chưa chạm middle) ──────► RESTART bar[0] làm trigger mới
    │
    ├─ low[0] < buyMinLow (đã chạm middle) ────────► IDLE (reset)
    │
    │ close[1]>open[1]  AND  high[1]>triggerHigh  AND  close[1]<=middle
    ▼
TRACKING (checkBuy=1, checkFirstbuy=1)
    │  "Chờ xác nhận đỉnh"
    │
    ├─ close[1] >= middle ──────────────────────────► IDLE (reset)
    │
    ├─ low[1] < buyMinLow ──────────────────────────► IDLE (reset, setup false)
    │
    │ close[2]>open[2]  AND  close[1]<open[1]  AND  filters OK
    ▼
ORDER (checkBuy=2)
    │  BuyStop đặt tại đỉnh + spread
    │
    ├─ low[0] < buyMinLow ──────────────────────────► Hủy lệnh, IDLE
    │
    ├─ SL hit ──────────────────────────────────────► IDLE (marks giữ lại)
    │
    └─ TP hit ──────────────────────────────────────► IDLE (marks xóa)
```

---

## Sơ đồ trạng thái SELL (đối xứng)

```
IDLE
    │ high[1] > UpperBB  AND  close[1] > middle
    ▼
TRACKING (checkFirstsell=0)  "Chờ nến giảm"
    │
    ├─ high[1] > sellMaxHigh ───────────────────────► IDLE (reset)
    ├─ high[0] > sellMaxHigh (chưa chạm middle) ───► RESTART bar[0]
    │
    │ close[1]<open[1]  AND  low[1]<triggerLow  AND  close[1]>=middle
    ▼
TRACKING (checkFirstsell=1)  "Chờ xác nhận đáy"
    │
    ├─ close[1] <= middle ──────────────────────────► IDLE (reset)
    ├─ high[1] > sellMaxHigh ───────────────────────► IDLE (setup false)
    │
    │ close[2]<open[2]  AND  close[1]>open[1]  AND  filters OK
    ▼
ORDER  →  SellStop đặt tại đáy
```

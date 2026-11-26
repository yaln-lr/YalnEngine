# 句柄风格代码 C++ 化重构方案（P0 + P1）

> 范围：P0（强类型句柄 + 强类型 PassMask）+ P1（配套 C++ 风格清理）。
>
> **实施状态：已完成（P0 + P1 全部落地）。**
> - `YalnHandle<Tag>` / `YalnPassMask` 已定义并替换全部调用点；
> - `isValid()` 已更名为 `contains()`，库内部裸索引已收敛为 `h.valid()` + `h.value()`；
> - `name()` / `initVulkan()` / `initialize()` / `create()` 改为 `std::string_view`，Vulkan C API 边界用局部 `std::string` 承接；
> - `YalnVertexView` 替代裸 `const void*` 数据视图（旧接口保留兼容）；
> - `(void)param` 改为 `[[maybe_unused]]`，`init()` 补齐返回值；
> - push constants 的字节转换收敛到 `asBytes()` helper；
> - 验收：编译零错误零警告，五种渲染模式运行无 Vulkan 校验错误。

---

## 1. 检查结论

### 1.1 核心判断

“句柄”本身不是 C 风格——现代 C++ 引擎（EnTT / flecs / ECS）大量使用句柄。
真正 C 风格的是目前这套实现：**裸 `uint32_t` + 宏式常量 + 隐式可互换 + 手工下标索引**。

`core/include/yalnrendertypes.h` 现状：

```cpp
using YalnPipelineHandle = uint32_t;
using YalnMaterialHandle = uint32_t;

inline constexpr YalnPipelineHandle YALN_INVALID_PIPELINE = 0xFFFFFFFFu;
inline constexpr YalnMaterialHandle YALN_INVALID_MATERIAL = 0xFFFFFFFFu;
inline constexpr YalnPipelineHandle YALN_DEFAULT_PIPELINE = 0u;
inline constexpr YalnMaterialHandle YALN_DEFAULT_MATERIAL = 0u;
```

### 1.2 四个 C 风格特征

| 问题 | 具体表现 |
|---|---|
| 无类型区分 | 材质句柄和管线句柄都是 `uint32_t`，可互相赋值/比较/传参，编译器不拦 |
| 无有效性语义 | `0xFFFFFFFF` 是裸魔数，任何整数都能充当句柄 |
| 手工索引 | `m_materials[handle]`、`m_entries[handle]` 直接拿整数当下标 |
| 常量命名宏化 | `YALN_INVALID_*` / `YALN_DEFAULT_*` 是常量却用宏式命名 |

### 1.3 影响范围

| 符号 | 出现次数 |
|---|---|
| `YalnMaterialHandle` | 48 |
| `YalnPipelineHandle` | 44 |
| `YALN_INVALID_PIPELINE` | 22 |
| `YALN_INVALID_MATERIAL` | 12 |
| `YALN_DEFAULT_PIPELINE` / `MATERIAL` | 15 |
| `toPassMask` / `toPassIndex` / `YALN_PASS_MASK_ALL` | 16 |

涉及约 15 个文件：

```text
core/include/yalnrendertypes.h        （定义源头）
core/include/yalninterfaces.h         （YalnDrawItem / IYalnRenderer）
core/include/yalnmateriallibrary.h    core/src/yalnmateriallibrary.cpp
core/include/yalnpipelinelibrary.h    core/src/yalnpipelinelibrary.cpp
core/include/yalndrawable.h           core/include/yalnnode.h
core/include/yalnview.h               core/src/yalnview.cpp
core/include/yalnrenderer.h           core/src/yalnrenderer.cpp
core/include/yalnframe.h              core/src/yalnforwardpass.cpp
viewer/include/yalnmodematerial.h     viewer/src/yalnmodematerial.cpp
viewer/include/yalnrendermodecontroller.h / src/...
viewer/src/mainwin.cpp
```

### 1.4 其他相关 C 风格写法（P1）

| 写法 | 位置 | 次数 | C++ 替代 |
|---|---|---|---|
| `const char* name()` | 各接口、`event.h` | 74 | `std::string_view` |
| `const void* getVertexData()` | `yalnmesh.h` | 9 | 类型化 view |
| `(void)param;` | 大量默认实现 | 多 | `[[maybe_unused]]` 或无名形参 |
| `reinterpret_cast<const uint8_t*>` | push constants | 2 | 局部 helper 收敛 |
| `virtual bool init(){};` | `yalnview.h` | 1 | 补 `return true;` |

**不建议改**：

- `YALN_CORE_EXPORT` 宏（DLL 导出必需）；
- Vulkan C API 调用处的 `const char*`（API 要求）；
- `handle()` 命名（Vulkan-Hpp RAII 惯例，不是 C 风格句柄）。

---

## 2. 重构目标

1. **类型安全**：材质句柄不能当管线句柄用；无效句柄不能当索引用。
2. **语义显式**：`handle.valid()` / `operator bool()` / `handle.value()` 取代魔数比较。
3. **零运行开销**：强类型句柄是值类型，编译后仍是 `uint32_t`。
4. **渐进迁移**：允许旧代码逐步改，不要求一次性全改完。
5. **保持句柄语义**：不改成裸指针，避免 `clear()` 后悬空；生命周期规则不变。

---

## 3. P0 方案一：Tag 强类型句柄

### 3.1 核心类型（`yalnrendertypes.h`）

```cpp
// 强类型句柄：零开销，编译后就是一个 uint32_t
template <class Tag, class Value = std::uint32_t>
class YalnHandle
{
public:
    using value_type = Value;

    static constexpr value_type invalid_value = static_cast<value_type>(-1);

    constexpr YalnHandle() noexcept = default;
    constexpr explicit YalnHandle(value_type v) noexcept : m_value(v) {}

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return m_value != invalid_value;
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return valid();
    }
    [[nodiscard]] constexpr value_type value() const noexcept
    {
        return m_value;
    }

    // C++17 不能对比较运算符用 = default，手写
    [[nodiscard]] friend constexpr bool operator==(
        YalnHandle a, YalnHandle b) noexcept
    {
        return a.m_value == b.m_value;
    }
    [[nodiscard]] friend constexpr bool operator!=(
        YalnHandle a, YalnHandle b) noexcept
    {
        return !(a == b);
    }
    [[nodiscard]] friend constexpr bool operator<(
        YalnHandle a, YalnHandle b) noexcept
    {
        return a.m_value < b.m_value;   // 供排序使用
    }

private:
    value_type m_value = invalid_value;
};

struct YalnPipelineTag;
struct YalnMaterialTag;

using YalnPipelineHandle = YalnHandle<YalnPipelineTag>;
using YalnMaterialHandle = YalnHandle<YalnMaterialTag>;

inline constexpr YalnPipelineHandle YALN_INVALID_PIPELINE{};
inline constexpr YalnMaterialHandle YALN_INVALID_MATERIAL{};
inline constexpr YalnPipelineHandle YALN_DEFAULT_PIPELINE{0u};
inline constexpr YalnMaterialHandle YALN_DEFAULT_MATERIAL{0u};
```

保留 `YALN_*` 常量名是为了降低迁移成本；若想更彻底，可改为静态成员函数：

```cpp
YalnMaterialHandle::invalid();
YalnMaterialHandle::defaultValue();
```

### 3.2 库侧改造

```cpp
class YalnMaterialLibrary
{
public:
    YalnMaterialHandle createMaterial(YalnMaterialDesc desc);

    YalnMaterial*       get(YalnMaterialHandle h) noexcept;
    const YalnMaterial* get(YalnMaterialHandle h) const noexcept;

    [[nodiscard]] bool contains(YalnMaterialHandle h) const noexcept;

private:
    std::vector<std::unique_ptr<YalnMaterial>> m_materials;
};
```

实现里把裸索引收敛到一处：

```cpp
YalnMaterial* YalnMaterialLibrary::get(YalnMaterialHandle h) noexcept
{
    if (!h.valid()) return nullptr;
    const auto index = static_cast<std::size_t>(h.value());
    return index < m_materials.size() ? m_materials[index].get() : nullptr;
}
```

`YalnPipelineLibrary` 同理（`get` / `isValid`→`contains` / `name`）。

`YalnMaterialDesc::passPipelines` 可简化：

```cpp
// 旧：要写 5 个 YALN_INVALID_PIPELINE
std::array<YalnPipelineHandle, YALN_PASS_SLOT_COUNT> passPipelines = {
    YALN_INVALID_PIPELINE, YALN_INVALID_PIPELINE, ... };

// 新：强类型句柄默认构造即 invalid
std::array<YalnPipelineHandle, YALN_PASS_SLOT_COUNT> passPipelines{};
```

### 3.3 P0 方案二：强类型 PassMask

```cpp
class YalnPassMask
{
public:
    using Bits = std::uint32_t;

    constexpr YalnPassMask() noexcept = default;
    constexpr explicit YalnPassMask(Bits bits) noexcept : m_bits(bits) {}

    [[nodiscard]] constexpr bool contains(YalnPassKind kind) const noexcept
    {
        return (m_bits & static_cast<Bits>(kind)) != 0;
    }
    [[nodiscard]] friend constexpr YalnPassMask operator|(
        YalnPassMask a, YalnPassMask b) noexcept
    {
        return YalnPassMask(a.m_bits | b.m_bits);
    }

private:
    Bits m_bits = 0;
};

constexpr YalnPassMask YALN_PASS_ALL{0xFFFFFFFFu};
constexpr YalnPassMask passMask(YalnPassKind k) noexcept
{
    return YalnPassMask(static_cast<YalnPassMask::Bits>(k));
}
```

调用点变化：

```cpp
// 旧
if ((item.passMask & passBit) == 0) continue;

// 新
if (!item.passMask.contains(pass)) continue;
```

`passBit` 这个裸 `uint32_t` 局部变量随之消失。

---

## 4. P1 配套清理

### 4.1 `const char*` → `std::string_view`

适用范围（**非拥有**字符串参数/返回值）：

```cpp
// 各接口
virtual std::string_view name() const;
bool initVulkan(std::string_view appName);
bool initialize(std::string_view appName);
const char* getName() const;   // event.h → std::string_view getName() const;
```

风险与对策：

> `std::string_view::data()` **不保证以 `'\0'` 结尾**。
> 传给 Vulkan C API（`vk::ApplicationInfo`、layer/extension 名称数组）时，
> 要么用字面量构造的 string_view（有 null 结尾），
> 要么在调用点局部构造 `std::string` 再取 `.c_str()`。
> 绝不能直接把任意 `string_view.data()` 当 C 字符串用。

### 4.2 `getVertexData()` → 类型化 view（C++17）

```cpp
struct YalnVertexView
{
    const std::byte* data   = nullptr;
    std::size_t      count  = 0;
    std::size_t      stride = 0;
};

class YalnMesh
{
public:
    virtual YalnVertexView vertexView() const
    {
        const auto& v = getVertices();
        return {
            reinterpret_cast<const std::byte*>(v.data()),
            v.size(),
            sizeof(YalnVertexPC)
        };
    }
};
```

`reinterpret_cast` 只保留在这一处，场景上传代码用 `view.data` / `view.count` / `view.stride`。

### 4.3 `(void)param` → `[[maybe_unused]]`

```cpp
// 旧
virtual bool initialize(const IYalnDevice& device,
                        const IYalnSwapChain& swapChain)
{
    (void)device;
    (void)swapChain;
    return true;
}

// 新
virtual bool initialize([[maybe_unused]] const IYalnDevice& device,
                        [[maybe_unused]] const IYalnSwapChain& swapChain)
{
    return true;
}
```

或直接省略形参名：`virtual bool initialize(const IYalnDevice&, const IYalnSwapChain&)`。

### 4.4 修复 `init()` warning

```cpp
// yalnview.h:138 现状（有 -Wreturn-type 警告）
virtual bool init(){};

// 改为
virtual bool init() { return true; }
```

### 4.5 push constants 的字节视图

把 `reinterpret_cast<const uint8_t*>(&constants)` 收敛到一个 helper：

```cpp
template <class T>
[[nodiscard]] const std::byte* asBytes(const T& value) noexcept
{
    return reinterpret_cast<const std::byte*>(&value);
}
```

> C++20 可直接用 `std::as_bytes(std::span(&value, 1))`；本项目是 C++17，用上面的 helper。

---

## 5. 分阶段迁移计划

### 阶段 1：定义新类型，保持兼容

- 在 `yalnrendertypes.h` 加入 `YalnHandle<Tag>` 和 `YalnPassMask`；
- `YalnMaterialHandle` / `YalnPipelineHandle` 改为新类型；
- 保留全部 `YALN_*` 常量名；
- 预期：编译错误集中在“把整数当句柄”和“`handle >= size()`”处。

### 阶段 2：修库与消费者

| 文件 | 改动 |
|---|---|
| `yalnmateriallibrary.cpp/.h` | `get()` 用 `h.valid()` + `h.value()`；`isValid` → `contains` |
| `yalnpipelinelibrary.cpp/.h` | 同上；`name(h)` 同上 |
| `yalnforwardpass.cpp` | 排序用 `handle.value()` 或 `operator<`；Pass 掩码用 `.contains()` |
| `yalnview.cpp` | 创建函数返回类型自动适配 |
| `yalnrenderer.cpp` | 同上 |
| `viewer/yalnmodematerial.*`、`yalnrendermodecontroller.*`、`mainwin.cpp` | 句柄比较改 `valid()` / `==` |

### 阶段 3：数据成员类型替换

- `passPipelines` 初始化简化为 `{}`；
- `YalnDrawable` / `YalnNode` / `YalnScene` 的材质字段自动跟随类型；
- `pipelineOverride` 默认值为强类型 `YALN_INVALID_PIPELINE`。

### 阶段 4：P1 配套清理

- `name()` → `std::string_view`，注意 C API 边界；
- `getVertexData()` → `YalnVertexView`；
- `(void)` → `[[maybe_unused]]`；
- `init()` 修复 warning；
- push constants 用 `asBytes()` helper。

### 阶段 5：文档与验收

- 更新三份文档中的 API 片段（渲染管线接口使用说明 / 类图与数据流 / 渲染模式与 MainWindow 重构）；
- 全量编译 + 五种渲染模式运行 + 校验层 0 错误；
- grep 确认无 `handle >= size()`、无整数当句柄、无裸 `0xFFFFFFFF`（除类型定义处）。

---

## 6. 备选方案对比

| 方案 | 类型安全 | 生命周期 | 改动量 | 评价 |
|---|---|---|---|---|
| **A. Tag 强类型句柄（推荐）** | ✅ 编译期区分材质/管线 | 由库管理，规则不变 | 中（~90 处） | 零开销、渐进、语义保留 |
| B. 裸指针 `YalnMaterial*` | 部分 | `clear()` 后悬空 ❌ | 大 | 不推荐 |
| C. `std::shared_ptr<YalnMaterial>` | ✅ | 安全但所有权复杂 | 很大 | 外部管线登记会变味，不推荐 |
| D. 保持现状 | ❌ | — | 0 | 不满足诉求 |

---

## 7. 风险与注意点

1. **ABI 变化**：句柄从 `uint32_t` 变为类，DLL 接口签名变化——core 与 viewer 必须同时重编（当前就是一起构建，OK）。
2. **排序稳定性**：比较器改用 `handle.value()` 或 `operator<`，行为与当前一致。
3. **`size()` 比较**：`h.value()` 是 `uint32_t`，`size()` 是 `size_t`，显式转换避免宽度/符号警告。
4. **默认句柄语义**：0 = 默认项、invalid = -1 的规则完全不变，保留常量名则调用逻辑无需改。
5. **外部管线生命周期**：`addExternal()` 规则不变；强类型句柄不解决也不恶化此问题。
6. **`string_view` 陷阱**：见 4.1，C API 边界必须显式处理 null 结尾。

---

## 8. 验收标准

- `YalnMaterialHandle` 与 `YalnPipelineHandle` 不能互相赋值（写一行测试代码应编译失败）；
- 无效句柄不再是裸 `0xFFFFFFFF`；
- 库实现不再写 `m_vec[handle]` 裸索引；
- PassMask 用 `.contains()` 而非裸位运算；
- 五种渲染模式行为不变，Vulkan 校验层无错误；
- `-Wreturn-type` 警告消失。

# PlandFly

`PlandFly` 是一个基于 LeviLamina 的服务器插件，用于给 `Pland` 领地系统增加“领地内飞行”功能，可按配置切换为付费或免费模式。

玩家可以先开启飞行待命状态。进入符合条件的领地后，插件会自动授予飞行能力，并按配置决定是否按时间扣除经济；离开领地后，飞行与收费都会自动暂停；重新进入领地后会自动恢复。

## 功能介绍

- 支持玩家在领地外先开启飞行待命状态。
- 进入可飞行领地后自动启用飞行。
- 离开领地后自动暂停飞行，并停止收费。
- 重新进入领地后自动恢复飞行，并继续或重新开始计费。
- 支持关闭经济模式，改为免费飞行。
- 支持按固定时间周期扣费。
- 支持 GUI 页面操作，不需要手打完整命令。
- 支持限制只有领地主人/成员才能使用。
- 尽量避免接管玩家原本已有的飞行权限。

## 依赖

本插件运行前需要以下插件已正常加载：

- `Pland`
- `LegacyMoney`

## 使用方式

### 命令

默认主命令：

```text
/plandfly
```

默认别名：

```text
/pfly
```

支持的子命令：

- `/plandfly`
  打开 GUI 主界面
- `/plandfly gui`
  打开 GUI 主界面
- `/plandfly on`
  开启飞行状态
- `/plandfly off`
  关闭飞行状态
- `/plandfly status`
  查看当前状态

### GUI

输入 `/plandfly` 后会打开主界面，界面中会显示：

- 当前是否处于待命/激活状态
- 当前所在领地
- 下次扣费时间
- 收费标准
- 当前经济余额

界面中提供以下按钮：

- 开启飞行状态
- 关闭飞行状态
- 发送文字状态
- 刷新界面

## 飞行与收费逻辑

玩家开启飞行状态后，插件按以下规则运行：

1. 如果玩家当前不在可飞行领地内：
   插件只记录“待命状态”，不会立即给予飞行，也不会收费。
2. 如果玩家进入可飞行领地：
   插件自动给予飞行，并根据配置开始收费。
3. 如果玩家离开领地：
   插件自动关闭由本插件授予的飞行，并暂停收费。
4. 如果玩家重新进入领地：
   插件自动恢复飞行，并继续计费。
5. 如果余额不足或扣费失败：
   插件会自动关闭这次飞行状态。
6. 如果玩家手动关闭：
   插件会结束待命/飞行状态。

## 默认配置

插件首次启动时会自动生成 `config/config.json`。

当前配置结构如下：

```json
{
    "version": 3,
    "logLevel": "Info",
    "language": "zh_CN",
    "landFlight": {
        "enabled": true,
        "command": "plandfly",
        "alias": "pfly",
        "useEconomy": true,
        "chargeAmount": 10,
        "chargeIntervalSeconds": 10,
        "chargeOnStart": true,
        "requireLandMember": true,
        "notifyEachCharge": true
    }
}
```

### 配置项说明

- `language`
  插件语言，支持 `zh_CN`、`en_US`
- `landFlight.enabled`
  是否启用领地飞行功能
- `landFlight.command`
  主命令名称
- `landFlight.alias`
  命令别名
- `landFlight.useEconomy`
  是否启用经济扣费；为 `false` 时改为免费飞行，下面几个收费相关配置将不再生效
- `landFlight.chargeAmount`
  每个收费周期扣除的经济数量，仅在 `landFlight.useEconomy = true` 时生效
- `landFlight.chargeIntervalSeconds`
  收费周期，单位为秒，仅在 `landFlight.useEconomy = true` 时生效
- `landFlight.chargeOnStart`
  玩家进入可飞行领地时，是否立即扣除首期费用，仅在 `landFlight.useEconomy = true` 时生效
- `landFlight.requireLandMember`
  是否要求玩家必须是领地主人或成员
- `landFlight.notifyEachCharge`
  每次成功扣费后是否发送提示，仅在 `landFlight.useEconomy = true` 时生效

## 安装

1. 确保服务器已安装 `LeviLamina`
2. 确保服务器已安装并正常加载：
   `Pland`、`LegacyMoney`
3. 将插件放入服务器插件目录
4. 启动服务器，等待插件自动生成配置文件
5. 根据需要修改 `config/config.json`
6. 重启服务器或重新加载插件


## 适用场景

- 生存服希望允许玩家在自己领地里付费飞行
- 希望把飞行能力限制在安全区域内
- 希望把飞行做成一种持续消耗型服务

## 说明

- 本插件只处理“由插件自己授予的飞行能力”。
- 玩家如果本来就拥有管理员飞行、创造飞行或其他插件授予的飞行，本插件不会强行接管。

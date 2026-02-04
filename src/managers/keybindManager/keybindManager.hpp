using namespace geode::prelude;

struct Keybind {
	int value;

    // no idea what any of this does but it was in the docs
	bool operator==(Keybind const& other) const = default;
	operator int() const { return value; };

    Keybind() = default;
    Keybind(int value) : value(value) {}
    Keybind(Keybind const&) = default;

    enumKeyCodes getKey() const { return static_cast<enumKeyCodes>(value); }
};

class KeyPicker : public SettingBaseValueV3<int> {
public:
    // all of this is copied from the docs because i have no idea what i'm doing
    static Result<std::shared_ptr<SettingV3>> parse(
        std::string const& key,
        std::string const& modID,
        matjson::Value const& json
    ) {
        auto res = std::make_shared<KeyPicker>();
        auto root = checkJson(json, "KeyPicker");
        res->parseBaseProperties(key, modID, root);
        return root.ok(std::static_pointer_cast<SettingV3>(res));
    }

    SettingNodeV3* createNode(float width) override;
};   

template <>
struct matjson::Serialize<Keybind> {
    static matjson::Value toJson(Keybind const& settingValue) { return settingValue.value; }
    static Result<Keybind> fromJson(matjson::Value const& json) {
        GEODE_UNWRAP_INTO(auto num, json.asInt());
        return Ok(Keybind(num));
    }
};

template <>
struct geode::SettingTypeForValueType<Keybind> {
    using SettingType = KeyPicker;
};
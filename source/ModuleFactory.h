//
// Created by Bence Kovács on 04/01/2024.
//
namespace PlayfulTones {
    class ModuleFactory final
    {
    public:
        using Constructor = std::function<std::unique_ptr<juce::AudioProcessor>()>;

        ModuleFactory(std::initializer_list<Constructor> constructors);
        ModuleFactory(const std::vector<Constructor>& constructors);
        ModuleFactory(std::unordered_map<int, Constructor> constructors);
        [[nodiscard]] juce::StringArray getNames() const;

        std::unique_ptr<juce::AudioProcessor> createProcessor(int index);
        [[nodiscard]] int getNumModules() const;

    private:
        const std::unordered_map<int, Constructor> constructors;

        template <typename Collection>
        static std::unordered_map<int, Constructor> createMapFromCollection(const Collection& collection) {
            std::unordered_map<int, Constructor> map;
            int index = 0;
            for (const auto& item : collection) {
                map.insert({index++, item});
            }
            return map;
        }
    };
} // namespace PlayfulTones
//
// Created by Bence Kovács on 04/01/2024.
//
namespace PlayfulTones {
    class ModuleFactory final
    {
    public:
        using Constructor = std::function<std::unique_ptr<juce::AudioProcessor>()>;

        ModuleFactory(std::initializer_list<Constructor> constructors);
        [[nodiscard]] juce::StringArray getNames() const;

        std::unique_ptr<juce::AudioProcessor> createProcessor(int index);
        [[nodiscard]] int getNumModules() const;

    private:
        const std::vector<Constructor> constructors;
    };
} // namespace PlayfulTones
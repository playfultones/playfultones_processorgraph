//
// Created by Bence Kovács on 04/01/2024.
//
namespace PlayfulTones {
    ModuleFactory::ModuleFactory(std::initializer_list<Constructor> cCollection)
        : constructors(cCollection)
    {
    }

    juce::StringArray ModuleFactory::getNames() const
    {
        juce::StringArray names;
        for (const auto& constructor : constructors)
            names.add(constructor()->getName());
        return names;
    }

    std::unique_ptr<juce::AudioProcessor> ModuleFactory::createProcessor(int index)
    {
        return constructors[static_cast<size_t>(index)]();
    }

    int ModuleFactory::getNumModules() const
    {
        return static_cast<int>(constructors.size());
    }
} // namespace PlayfulTones
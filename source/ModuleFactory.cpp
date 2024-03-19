//
// Created by Bence Kovács on 04/01/2024.
//
namespace PlayfulTones {
    ModuleFactory::ModuleFactory(std::initializer_list<Constructor> cCollection)
            : constructors(createMapFromCollection(cCollection))
    {
    }

    ModuleFactory::ModuleFactory(const std::vector<Constructor>& cCollection)
            : constructors(createMapFromCollection(cCollection))
    {
    }

    ModuleFactory::ModuleFactory(std::unordered_map<int, Constructor> cCollection)
            : constructors(std::move(cCollection))
    {
    }

    juce::StringArray ModuleFactory::getNames() const
    {
        juce::StringArray names;
        for (const auto& pair : constructors)
            names.add(pair.second()->getName());
        return names;
    }

    std::unique_ptr<juce::AudioProcessor> ModuleFactory::createProcessor(int index)
    {
        if (auto it = constructors.find(index); it != constructors.end())
            return it->second();
        return nullptr;
    }

    int ModuleFactory::getNumModules() const
    {
        return static_cast<int>(constructors.size());
    }
} // namespace PlayfulTones
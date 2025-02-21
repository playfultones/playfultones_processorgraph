//
// Created by Bence Kovács on 04/01/2024.
//

namespace PlayfulTones {
    ProcessorGraph::ProcessorGraph (ModuleFactory f, GuiConfig guiC)
        : factory (std::move(f)), guiConfig(guiC)
    {
    }

    ProcessorGraph::~ProcessorGraph()
    {
        clear();
    }

    void ProcessorGraph::setNodePosition (NodeID nodeID, Point<double> pos) const
    {
        if (auto* n = graph.getNodeForId (nodeID))
        {
            n->properties.set (xPosId, jlimit (0.0, 1.0, pos.x));
            n->properties.set (yPosId, jlimit (0.0, 1.0, pos.y));
        }
    }

    Point<double> ProcessorGraph::getNodePosition (NodeID nodeID) const
    {
        if (auto* n = graph.getNodeForId (nodeID))
            return { static_cast<double> (n->properties [xPosId]),
                     static_cast<double> (n->properties [yPosId]) };

        return {};
    }

    //==============================================================================
    void ProcessorGraph::clear()
    {
        graphListeners.call(&Listener::graphIsAboutToBeCleared);
        graph.clear();
        factoryIdToNextInstanceIdMap.clear();
    }

    //==============================================================================
    static void readBusLayoutFromXml (AudioProcessor::BusesLayout& busesLayout, AudioProcessor& plugin,
                                      const XmlElement& xml, bool isInput)
    {
        auto& targetBuses = (isInput ? busesLayout.inputBuses
                                     : busesLayout.outputBuses);
        int maxNumBuses = 0;

        if (auto* buses = xml.getChildByName (isInput ? ProcessorGraph::inputsAttrName : ProcessorGraph::outputsAttrName))
        {
            for (auto* e : buses->getChildWithTagNameIterator (ProcessorGraph::busAttrName))
            {
                const int busIdx = e->getIntAttribute (ProcessorGraph::indexAttrName);
                maxNumBuses = jmax (maxNumBuses, busIdx + 1);

                // the number of buses on busesLayout may not be in sync with the plugin after adding buses
                // because adding an input bus could also add an output bus
                for (int actualIdx = plugin.getBusCount (isInput) - 1; actualIdx < busIdx; ++actualIdx)
                    if (! plugin.addBus (isInput))
                        return;

                for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                    targetBuses.add (plugin.getChannelLayoutOfBus (isInput, busIdx));

                auto layout = e->getStringAttribute (ProcessorGraph::layoutAttrName);

                if (layout.isNotEmpty())
                    targetBuses.getReference (busIdx) = AudioChannelSet::fromAbbreviatedString (layout);
            }
        }

        // if the plugin has more buses than specified in the xml, then try to remove them!
        while (maxNumBuses < targetBuses.size())
        {
            if (! plugin.removeBus (isInput))
                return;

            targetBuses.removeLast();
        }
    }

    //==============================================================================
    static XmlElement* createBusLayoutXml (const AudioProcessor::BusesLayout& layout, const bool isInput)
    {
        auto& buses = isInput ? layout.inputBuses
                              : layout.outputBuses;

        auto* xml = new XmlElement (isInput ? ProcessorGraph::inputsAttrName : ProcessorGraph::outputsAttrName);

        for (int busIdx = 0; busIdx < buses.size(); ++busIdx)
        {
            auto& set = buses.getReference (busIdx);

            auto* bus = xml->createNewChildElement (ProcessorGraph::busAttrName);
            bus->setAttribute (ProcessorGraph::indexAttrName, busIdx);
            bus->setAttribute (ProcessorGraph::layoutAttrName, set.isDisabled() ? ProcessorGraph::disabledAttrValue : set.getSpeakerArrangementAsString());
        }

        return xml;
    }

    static XmlElement* createNodeXml (AudioProcessorGraph::Node* const node) noexcept
    {
        auto* processor = node->getProcessor();
        auto e = new XmlElement (ProcessorGraph::filterAttrName);

        auto* uidElement = e->createNewChildElement (ProcessorGraph::propertyAttrName);
        uidElement->setAttribute (ProcessorGraph::nameTag, ProcessorGraph::nodeId);
        uidElement->setAttribute (ProcessorGraph::typeTag, ProcessorGraph::intValue);
        uidElement->setAttribute (ProcessorGraph::valueTag, (int) node->nodeID.uid);

        for(const auto& prop : node->properties)
        {
            auto* propElement = e->createNewChildElement (ProcessorGraph::propertyAttrName);
            propElement->setAttribute (ProcessorGraph::nameTag, prop.name.toString());
            if(prop.value.isInt())
            {
                propElement->setAttribute (ProcessorGraph::typeTag, ProcessorGraph::intValue);
                propElement->setAttribute (ProcessorGraph::valueTag, prop.value.toString());
            }
            else if(prop.value.isDouble())
            {
                propElement->setAttribute (ProcessorGraph::typeTag, ProcessorGraph::floatValue);
                propElement->setAttribute (ProcessorGraph::valueTag, prop.value.toString());
            }
            else if(prop.value.isString())
            {
                propElement->setAttribute (ProcessorGraph::typeTag, ProcessorGraph::stringValue);
                propElement->setAttribute (ProcessorGraph::valueTag, prop.value.toString());
            }
            else if(prop.value.isBool())
            {
                propElement->setAttribute (ProcessorGraph::typeTag, ProcessorGraph::boolValue);
                propElement->setAttribute (ProcessorGraph::valueTag, prop.value.toString());
            }
        }

        for (int i = (int)ModuleWindow::Type::first; i <= (int)ModuleWindow::Type::last; ++i)
        {
            auto type = (ModuleWindow::Type) i;

            if (node->properties.contains (ModuleWindow::getOpenProp (type)))
            {
                e->setAttribute (ModuleWindow::getLastXProp (type), node->properties[ModuleWindow::getLastXProp (type)].toString());
                e->setAttribute (ModuleWindow::getLastYProp (type), node->properties[ModuleWindow::getLastYProp (type)].toString());
                e->setAttribute (ModuleWindow::getOpenProp (type),  node->properties[ModuleWindow::getOpenProp (type)].toString());
            }
        }

        MemoryBlock m;
        node->getProcessor()->getStateInformation (m);
        e->createNewChildElement (ProcessorGraph::stateAttrName)->addTextElement (m.toBase64Encoding());

        auto layout = processor->getBusesLayout();
        auto layouts = e->createNewChildElement (ProcessorGraph::layoutAttrName);
        layouts->addChildElement (createBusLayoutXml (layout, true));
        layouts->addChildElement (createBusLayoutXml (layout, false));

        return e;
    }

    AudioProcessorGraph::Node::Ptr ProcessorGraph::createNodeFromXml(const XmlElement& xml)
    {
        const auto properties = xml.getChildWithTagNameIterator(ProcessorGraph::propertyAttrName);

        if (properties == nullptr)
            return nullptr;

        auto uid = -1;
        auto factoryIndex = -1;
        for(auto* propElement : properties)
        {
            auto name = propElement->getStringAttribute(ProcessorGraph::nameTag);
            if(name == ProcessorGraph::nodeId)
                uid = static_cast<int>(propElement->getIntAttribute(ProcessorGraph::valueTag));
            else if(name == ProcessorGraph::factoryId)
                factoryIndex = propElement->getIntAttribute(ProcessorGraph::valueTag);
        }

        if(uid == -1 || factoryIndex == -1)
            return nullptr;

        auto processor = factory.createProcessor(factoryIndex);

        if (processor == nullptr)
            return nullptr;

        AudioProcessor::BusesLayout layout = processor->getBusesLayout();

        if (auto* layoutElement = xml.getChildByName(ProcessorGraph::layoutAttrName))
        {
            readBusLayoutFromXml(layout, *processor, *layoutElement, true);
            readBusLayoutFromXml(layout, *processor, *layoutElement, false);
        }

        processor->setBusesLayout(layout);

        if (auto* stateElement = xml.getChildByName(ProcessorGraph::stateAttrName))
        {
            MemoryBlock state;
            state.fromBase64Encoding(stateElement->getAllSubText());
            processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        }

        if (auto node = graph.addNode(std::move(processor), NodeID(static_cast<uint32>(uid))))
        {
            for(auto* propElement : properties)
            {
                auto name = propElement->getStringAttribute(ProcessorGraph::nameTag);
                auto type = propElement->getStringAttribute(ProcessorGraph::typeTag);
                auto var = juce::var();
                if(type == ProcessorGraph::intValue)
                    var = propElement->getIntAttribute(ProcessorGraph::valueTag);
                else if(type == ProcessorGraph::floatValue)
                    var = propElement->getDoubleAttribute(ProcessorGraph::valueTag);
                else if(type == ProcessorGraph::stringValue)
                    var = propElement->getStringAttribute(ProcessorGraph::valueTag);
                else if(type == ProcessorGraph::boolValue)
                    var = propElement->getBoolAttribute(ProcessorGraph::valueTag);
                node->properties.set(name, var);
            }

            for (int i = (int)ModuleWindow::Type::first; i <= (int)ModuleWindow::Type::last; ++i)
            {
                auto type = (ModuleWindow::Type) i;

                if (xml.hasAttribute (ModuleWindow::getOpenProp (type)))
                {
                    node->properties.set (ModuleWindow::getLastXProp (type), xml.getIntAttribute (ModuleWindow::getLastXProp (type)));
                    node->properties.set (ModuleWindow::getLastYProp (type), xml.getIntAttribute (ModuleWindow::getLastYProp (type)));
                    node->properties.set (ModuleWindow::getOpenProp  (type), xml.getIntAttribute (ModuleWindow::getOpenProp (type)));

                    if (node->properties[ModuleWindow::getOpenProp (type)])
                    {
                        jassert (node->getProcessor() != nullptr);

                        if(onProcessorWindowRequested != nullptr)
                            if(auto* w = onProcessorWindowRequested(node, type))
                                w->toFront (true);
                    }
                }
            }

            return node;
        }

        return nullptr;
    }

    std::unique_ptr<XmlElement> ProcessorGraph::createXml() const
    {
        auto xml = std::make_unique<XmlElement> (ProcessorGraph::graphAttrName);

        for (auto* node : graph.getNodes())
            xml->addChildElement (createNodeXml (node));

        for (auto& connection : graph.getConnections())
        {
            auto e = xml->createNewChildElement (ProcessorGraph::connectionAttrName);

            e->setAttribute (ProcessorGraph::srcFilterAttrName, (int) connection.source.nodeID.uid);
            e->setAttribute (ProcessorGraph::srcChannelAttrName, connection.source.channelIndex);
            e->setAttribute (ProcessorGraph::dstFilterAttrName, (int) connection.destination.nodeID.uid);
            e->setAttribute (ProcessorGraph::dstChannelAttrName, connection.destination.channelIndex);
        }

        return xml;
    }

    void ProcessorGraph::restoreFromXml(const XmlElement& xmlElement)
    {
        if (!xmlElement.hasTagName(ProcessorGraph::graphAttrName)) return;

        restoredState = XmlElement(xmlElement);

        struct ConnectionConfig {
            int srcFilter;
            int srcChannel;
            int dstFilter;
            int dstChannel;
        };

        std::vector<ConnectionConfig> restoredConnections;
        for (auto* connectionElement : restoredState.getChildWithTagNameIterator(ProcessorGraph::connectionAttrName))
        {
            restoredConnections.push_back({
                connectionElement->getIntAttribute(ProcessorGraph::srcFilterAttrName),
                connectionElement->getIntAttribute(ProcessorGraph::srcChannelAttrName),
                connectionElement->getIntAttribute(ProcessorGraph::dstFilterAttrName),
                connectionElement->getIntAttribute(ProcessorGraph::dstChannelAttrName)
            });
        }

        clear();
        for (auto* filterElement : restoredState.getChildWithTagNameIterator(ProcessorGraph::filterAttrName))
        {
            auto node = createNodeFromXml(*filterElement);
            if(node != nullptr)
                graphListeners.call(&Listener::nodeAdded, node->nodeID);
        }
        for (auto& conn : restoredConnections)
        {
            const auto connection = AudioProcessorGraph::Connection{
                {NodeID(static_cast<uint32>(conn.srcFilter)), conn.srcChannel},
                {NodeID(static_cast<uint32>(conn.dstFilter)), conn.dstChannel}
            };
            addConnection(connection);
        }
        graph.removeIllegalConnections();
    }

    void ProcessorGraph::addConnection (const AudioProcessorGraph::Connection& connection)
    {
        graph.addConnection (connection);
        graphListeners.call(&Listener::connectionAdded, connection);
    }

    void ProcessorGraph::removeConnection (const AudioProcessorGraph::Connection& connection)
    {
        graph.removeConnection (connection);
        graphListeners.call(&Listener::connectionRemoved, connection);
    }

    void ProcessorGraph::removeNode (NodeID nodeID)
    {
        if (auto* node = graph.getNodeForId (nodeID))
        {
            graph.removeNode (node);
            graphListeners.call(&Listener::nodeRemoved, nodeID);
        }
    }

    void ProcessorGraph::removeNode (const AudioProcessorGraph::Node::Ptr& node)
    {
        if (node != nullptr)
            removeNode (node->nodeID);
    }

    void ProcessorGraph::disconnectNode (NodeID nodeID)
    {
        if (graph.getNodeForId (nodeID) == nullptr)
            return;

        const auto connections = graph.getConnections();
        graph.disconnectNode(nodeID);
        for (const auto& c : connections)
            if (c.source.nodeID == nodeID || c.destination.nodeID == nodeID)
                graphListeners.call(&Listener::connectionRemoved, c);
    }

    void ProcessorGraph::disconnectNode (const AudioProcessorGraph::Node::Ptr& node)
    {
        if (node != nullptr)
            disconnectNode (node->nodeID);
    }

    juce::AudioProcessorGraph::Node::Ptr ProcessorGraph::createModule (int factoryIndex, double x, double y)
    {
        auto node = graph.addNode (factory.createProcessor(factoryIndex));
        if(node == nullptr)
            return nullptr;
        node->getProcessor()->enableAllBuses();
        node->properties.set (xPosId, x);
        node->properties.set (yPosId, y);
        node->properties.set(factoryId, factoryIndex);
        node->properties.set(instanceId, getNextInstanceId(factoryIndex));
        graphListeners.call(&Listener::nodeAdded, node->nodeID);
        return node;
    }

    void ProcessorGraph::addListener (ProcessorGraph::Listener* newListener)
    {
        graphListeners.add (newListener);
    }

    void ProcessorGraph::removeListener (ProcessorGraph::Listener* listener)
    {
        graphListeners.remove (listener);
    }

    int ProcessorGraph::getNextInstanceId(int factoryIndex) {
        if (factoryIdToNextInstanceIdMap.find(factoryIndex) == factoryIdToNextInstanceIdMap.end()) {
            factoryIdToNextInstanceIdMap[factoryIndex] = 0;
        }
        return factoryIdToNextInstanceIdMap[factoryIndex]++;
    }
} // namespace PlayfulTones
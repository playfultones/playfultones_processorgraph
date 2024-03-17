//
// Created by Bence Kovács on 04/01/2024.
//

namespace PlayfulTones {
    PluginGraph::PluginGraph (ModuleFactory& f, GuiConfig guiC)
        : factory (f), guiConfig(guiC)
    {
    }

    PluginGraph::~PluginGraph()
    {
        clear();
    }

    void PluginGraph::setNodePosition (NodeID nodeID, Point<double> pos) const
    {
        if (auto* n = graph.getNodeForId (nodeID))
        {
            n->properties.set (xPosId, jlimit (0.0, 1.0, pos.x));
            n->properties.set (yPosId, jlimit (0.0, 1.0, pos.y));
        }
    }

    Point<double> PluginGraph::getNodePosition (NodeID nodeID) const
    {
        if (auto* n = graph.getNodeForId (nodeID))
            return { static_cast<double> (n->properties [xPosId]),
                     static_cast<double> (n->properties [yPosId]) };

        return {};
    }

    //==============================================================================
    void PluginGraph::clear()
    {
        graphListeners.call(&Listener::graphIsAboutToBeCleared);
        graph.clear();
    }

    //==============================================================================
    static void readBusLayoutFromXml (AudioProcessor::BusesLayout& busesLayout, AudioProcessor& plugin,
                                      const XmlElement& xml, bool isInput)
    {
        auto& targetBuses = (isInput ? busesLayout.inputBuses
                                     : busesLayout.outputBuses);
        int maxNumBuses = 0;

        if (auto* buses = xml.getChildByName (isInput ? PluginGraph::inputsAttrName : PluginGraph::outputsAttrName))
        {
            for (auto* e : buses->getChildWithTagNameIterator (PluginGraph::busAttrName))
            {
                const int busIdx = e->getIntAttribute (PluginGraph::indexAttrName);
                maxNumBuses = jmax (maxNumBuses, busIdx + 1);

                // the number of buses on busesLayout may not be in sync with the plugin after adding buses
                // because adding an input bus could also add an output bus
                for (int actualIdx = plugin.getBusCount (isInput) - 1; actualIdx < busIdx; ++actualIdx)
                    if (! plugin.addBus (isInput))
                        return;

                for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                    targetBuses.add (plugin.getChannelLayoutOfBus (isInput, busIdx));

                auto layout = e->getStringAttribute (PluginGraph::layoutAttrName);

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

        auto* xml = new XmlElement (isInput ? PluginGraph::inputsAttrName : PluginGraph::outputsAttrName);

        for (int busIdx = 0; busIdx < buses.size(); ++busIdx)
        {
            auto& set = buses.getReference (busIdx);

            auto* bus = xml->createNewChildElement (PluginGraph::busAttrName);
            bus->setAttribute (PluginGraph::indexAttrName, busIdx);
            bus->setAttribute (PluginGraph::layoutAttrName, set.isDisabled() ? PluginGraph::disabledAttrValue : set.getSpeakerArrangementAsString());
        }

        return xml;
    }

    static XmlElement* createNodeXml (AudioProcessorGraph::Node* const node) noexcept
    {
        auto* processor = node->getProcessor();
        auto e = new XmlElement (PluginGraph::filterAttrName);

        e->setAttribute (PluginGraph::nodeId,           (int) node->nodeID.uid);
        e->setAttribute (PluginGraph::xPosId,           node->properties [PluginGraph::xPosId].toString());
        e->setAttribute (PluginGraph::yPosId,           node->properties [PluginGraph::yPosId].toString());
        e->setAttribute (PluginGraph::factoryId,        node->properties [PluginGraph::factoryId].toString());

        for (int i = (int)PluginWindow::Type::first; i <= (int)PluginWindow::Type::last; ++i)
        {
            auto type = (PluginWindow::Type) i;

            if (node->properties.contains (PluginWindow::getOpenProp (type)))
            {
                e->setAttribute (PluginWindow::getLastXProp (type), node->properties[PluginWindow::getLastXProp (type)].toString());
                e->setAttribute (PluginWindow::getLastYProp (type), node->properties[PluginWindow::getLastYProp (type)].toString());
                e->setAttribute (PluginWindow::getOpenProp (type),  node->properties[PluginWindow::getOpenProp (type)].toString());
            }
        }

        MemoryBlock m;
        node->getProcessor()->getStateInformation (m);
        e->createNewChildElement (PluginGraph::stateAttrName)->addTextElement (m.toBase64Encoding());

        auto layout = processor->getBusesLayout();
        auto layouts = e->createNewChildElement (PluginGraph::layoutAttrName);
        layouts->addChildElement (createBusLayoutXml (layout, true));
        layouts->addChildElement (createBusLayoutXml (layout, false));

        return e;
    }

    AudioProcessorGraph::Node::Ptr PluginGraph::createNodeFromXml(const XmlElement& xml)
    {
        auto uid = (uint32)xml.getIntAttribute(nodeId);
        auto factoryIndex = xml.getIntAttribute(factoryId);
        auto x = xml.getDoubleAttribute(xPosId);
        auto y = xml.getDoubleAttribute(yPosId);

        auto processor = factory.createProcessor(factoryIndex);

        if (processor == nullptr)
            return nullptr;

        AudioProcessor::BusesLayout layout = processor->getBusesLayout();

        if (auto* layoutElement = xml.getChildByName(PluginGraph::layoutAttrName))
        {
            readBusLayoutFromXml(layout, *processor, *layoutElement, true);
            readBusLayoutFromXml(layout, *processor, *layoutElement, false);
        }

        processor->setBusesLayout(layout);

        if (auto* stateElement = xml.getChildByName(PluginGraph::stateAttrName))
        {
            MemoryBlock state;
            state.fromBase64Encoding(stateElement->getAllSubText());
            processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        }

        if (auto node = graph.addNode(std::move(processor), NodeID(uid)))
        {
            node->properties.set(xPosId, x);
            node->properties.set(yPosId, y);
            node->properties.set(factoryId, factoryId);

            for (int i = (int)PluginWindow::Type::first; i <= (int)PluginWindow::Type::last; ++i)
            {
                auto type = (PluginWindow::Type) i;

                if (xml.hasAttribute (PluginWindow::getOpenProp (type)))
                {
                    node->properties.set (PluginWindow::getLastXProp (type), xml.getIntAttribute (PluginWindow::getLastXProp (type)));
                    node->properties.set (PluginWindow::getLastYProp (type), xml.getIntAttribute (PluginWindow::getLastYProp (type)));
                    node->properties.set (PluginWindow::getOpenProp  (type), xml.getIntAttribute (PluginWindow::getOpenProp (type)));

                    if (node->properties[PluginWindow::getOpenProp (type)])
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

    std::unique_ptr<XmlElement> PluginGraph::createXml() const
    {
        auto xml = std::make_unique<XmlElement> (PluginGraph::graphAttrName);

        for (auto* node : graph.getNodes())
            xml->addChildElement (createNodeXml (node));

        for (auto& connection : graph.getConnections())
        {
            auto e = xml->createNewChildElement (PluginGraph::connectionAttrName);

            e->setAttribute (PluginGraph::srcFilterAttrName, (int) connection.source.nodeID.uid);
            e->setAttribute (PluginGraph::srcChannelAttrName, connection.source.channelIndex);
            e->setAttribute (PluginGraph::dstFilterAttrName, (int) connection.destination.nodeID.uid);
            e->setAttribute (PluginGraph::dstChannelAttrName, connection.destination.channelIndex);
        }

        return xml;
    }

    void PluginGraph::restoreFromXml(const XmlElement& xmlElement)
    {
        if (!xmlElement.hasTagName(PluginGraph::graphAttrName)) return;

        restoredState = XmlElement(xmlElement);

        juce::MessageManager::callAsync([this]()
        {
            struct ConnectionConfig {
                int srcFilter;
                int srcChannel;
                int dstFilter;
                int dstChannel;
            };

            std::vector<ConnectionConfig> restoredConnections;
            for (auto* connectionElement : restoredState.getChildWithTagNameIterator(PluginGraph::connectionAttrName))
            {
                restoredConnections.push_back({
                    connectionElement->getIntAttribute(PluginGraph::srcFilterAttrName),
                    connectionElement->getIntAttribute(PluginGraph::srcChannelAttrName),
                    connectionElement->getIntAttribute(PluginGraph::dstFilterAttrName),
                    connectionElement->getIntAttribute(PluginGraph::dstChannelAttrName)
                });
            }

            clear();
            for (auto* filterElement : restoredState.getChildWithTagNameIterator(PluginGraph::filterAttrName))
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
        });
    }

    void PluginGraph::addConnection (const AudioProcessorGraph::Connection& connection)
    {
        graph.addConnection (connection);
        graphListeners.call(&Listener::connectionAdded, connection);
    }

    void PluginGraph::removeConnection (const AudioProcessorGraph::Connection& connection)
    {
        graph.removeConnection (connection);
        graphListeners.call(&Listener::connectionRemoved, connection);
    }

    void PluginGraph::removeNode (NodeID nodeID)
    {
        if (auto* node = graph.getNodeForId (nodeID))
        {
            graph.removeNode (node);
            graphListeners.call(&Listener::nodeRemoved, nodeID);
        }
    }

    void PluginGraph::removeNode (const AudioProcessorGraph::Node::Ptr& node)
    {
        if (node != nullptr)
            removeNode (node->nodeID);
    }

    void PluginGraph::disconnectNode (NodeID nodeID)
    {
        if (graph.getNodeForId (nodeID) == nullptr)
            return;

        const auto connections = graph.getConnections();
        graph.disconnectNode(nodeID);
        for (const auto& c : connections)
            if (c.source.nodeID == nodeID || c.destination.nodeID == nodeID)
                graphListeners.call(&Listener::connectionRemoved, c);
    }

    void PluginGraph::disconnectNode (const AudioProcessorGraph::Node::Ptr& node)
    {
        if (node != nullptr)
            disconnectNode (node->nodeID);
    }

    void PluginGraph::createPlugin (int factoryIndex, double x, double y)
    {
        if (factoryIndex < factory.getNumModules())
        {
            auto node = graph.addNode (factory.createProcessor(factoryIndex));
            node->getProcessor()->enableAllBuses();
            node->properties.set (xPosId, x);
            node->properties.set (yPosId, y);
            node->properties.set(factoryId, factoryIndex);
            graphListeners.call(&Listener::nodeAdded, node->nodeID);
        }
    }

    void PluginGraph::addListener (PluginGraph::Listener* newListener)
    {
        graphListeners.add (newListener);
    }

    void PluginGraph::removeListener (PluginGraph::Listener* listener)
    {
        graphListeners.remove (listener);
    }
} // namespace PlayfulTones
//
// Created by Bence Kovács on 04/01/2024.
//
namespace PlayfulTones {
    //==============================================================================
    struct GraphEditorPanel::PinComponent final : public Component,
                                                  public SettableTooltipClient
    {
        PinComponent (GraphEditorPanel& p, AudioProcessorGraph::NodeAndChannel pinToUse, bool isIn)
            : panel (p), graph (p.graph), pin (pinToUse), isInput (isIn)
        {
            if (auto node = graph.graph.getNodeForId (pin.nodeID))
            {
                String tip;

                if (pin.isMIDI())
                {
                    tip = isInput ? "MIDI Input"
                                  : "MIDI Output";
                }
                else
                {
                    auto& processor = *node->getProcessor();
                    auto channel = processor.getOffsetInBusBufferForAbsoluteChannelIndex (isInput, pin.channelIndex, busIdx);

                    if (auto* bus = processor.getBus (isInput, busIdx))
                        tip = bus->getName() + ": " + AudioChannelSet::getAbbreviatedChannelTypeName (bus->getCurrentLayout().getTypeOfChannel (channel));
                    else
                        tip = (isInput ? "Main Input: "
                                       : "Main Output: ") + String (pin.channelIndex + 1);

                }

                SettableTooltipClient::setTooltip (tip);
            }

            setSize (16, 16);
        }

        void paint (Graphics& g) override
        {
            auto w = (float) getWidth();
            auto h = (float) getHeight();

            Path p;
            p.addEllipse (w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
            p.addRectangle (w * 0.4f, isInput ? (0.5f * h) : 0.0f, w * 0.2f, h * 0.5f);

            auto colour = (pin.isMIDI() ? Colours::red : Colours::green);

            g.setColour (colour.withRotatedHue ((float) busIdx / 5.0f));
            g.fillPath (p);
        }

        void mouseDown (const MouseEvent& e) override
        {
            AudioProcessorGraph::NodeAndChannel dummy { {}, 0 };

            panel.beginConnectorDrag (isInput ? dummy : pin,
                isInput ? pin : dummy,
                e);
        }

        void mouseDrag (const MouseEvent& e) override
        {
            panel.dragConnector (e);
        }

        void mouseUp (const MouseEvent& e) override
        {
            panel.endDraggingConnector (e);
        }

        GraphEditorPanel& panel;
        ProcessorGraph& graph;
        AudioProcessorGraph::NodeAndChannel pin;
        const bool isInput;
        int busIdx = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinComponent)
    };

    //==============================================================================
    struct GraphEditorPanel::PluginComponent final : public Component,
                                                     private AudioProcessorParameter::Listener,
                                                     private AsyncUpdater
    {
        PluginComponent (GraphEditorPanel& p, AudioProcessorGraph::NodeID id)  : panel (p), graph (p.graph), pluginID (id)
        {
            shadow.setShadowProperties (DropShadow (Colours::black.withAlpha (0.5f), 3, { 0, 1 }));
            setComponentEffect (&shadow);

            if (auto f = graph.graph.getNodeForId (pluginID))
            {
                if (auto* processor = f->getProcessor())
                {
                    if (auto* bypassParam = processor->getBypassParameter())
                        bypassParam->addListener (this);
                }
            }

            setSize (150, 60);
        }

        PluginComponent (const PluginComponent&) = delete;
        PluginComponent& operator= (const PluginComponent&) = delete;

        ~PluginComponent() override
        {
            if (auto f = graph.graph.getNodeForId (pluginID))
            {
                if (auto* processor = f->getProcessor())
                {
                    if (auto* bypassParam = processor->getBypassParameter())
                        bypassParam->removeListener (this);
                }
            }
        }

        void mouseDown (const MouseEvent& e) override
        {
            originalPos = localPointToGlobal (Point<int>());

            toFront (true);

            if (e.mods.isPopupMenu() && graph.guiConfig.enableProcessorContextMenu)
                    showPopupMenu();
        }

        void mouseDrag (const MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu())
            {
                auto pos = originalPos + e.getOffsetFromDragStart();

                if (getParentComponent() != nullptr)
                    pos = getParentComponent()->getLocalPoint (nullptr, pos);

                pos += getLocalBounds().getCentre();

                graph.setNodePosition (pluginID,
                    { pos.x / (double) getParentWidth(),
                        pos.y / (double) getParentHeight() });

                panel.updateComponents();
            }
        }

        void mouseUp (const MouseEvent& e) override
        {
            if (e.mouseWasDraggedSinceMouseDown())
            {
                graph.graph.sendChangeMessage();
            }
            else if (e.getNumberOfClicks() == 2 && graph.guiConfig.enableProcessorEditorCreation)
            {
                if (auto f = graph.graph.getNodeForId (pluginID))
                    if (auto* w = panel.getOrCreateWindowFor (f, ModuleWindow::Type::normal))
                        w->toFront (true);
            }
        }

        bool hitTest (int x, int y) override
        {
            for (auto* child : getChildren())
                if (child->getBounds().contains (x, y))
                    return true;

            return x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize;
        }

        void paint (Graphics& g) override
        {
            auto boxArea = getLocalBounds().reduced (4, pinSize);
            bool isBypassed = false;

            if (auto* f = graph.graph.getNodeForId (pluginID))
                isBypassed = f->isBypassed();

            auto boxColour = findColour (TextEditor::backgroundColourId);

            if (isBypassed)
                boxColour = boxColour.brighter();

            g.setColour (boxColour);
            g.fillRect (boxArea.toFloat());

            g.setColour (findColour (TextEditor::textColourId));
            g.setFont (font);
            g.drawFittedText (getName(), boxArea, Justification::centred, 2);
        }

        void resized() override
        {
            if (auto f = graph.graph.getNodeForId (pluginID))
            {
                if (auto* processor = f->getProcessor())
                {
                    for (auto* pin : pins)
                    {
                        const bool isInput = pin->isInput;
                        auto channelIndex = pin->pin.channelIndex;
                        int busIdx = 0;
                        processor->getOffsetInBusBufferForAbsoluteChannelIndex (isInput, channelIndex, busIdx);

                        const int total = isInput ? numIns : numOuts;
                        const int index = pin->pin.isMIDI() ? (total - 1) : channelIndex;

                        auto totalSpaces = static_cast<float> (total) + (static_cast<float> (jmax (0, processor->getBusCount (isInput) - 1)) * 0.5f);
                        auto indexPos = static_cast<float> (index) + (static_cast<float> (busIdx) * 0.5f);

                        pin->setBounds (proportionOfWidth ((1.0f + indexPos) / (totalSpaces + 1.0f)) - pinSize / 2,
                            pin->isInput ? 0 : (getHeight() - pinSize),
                            pinSize, pinSize);
                    }
                }
            }
        }

        [[nodiscard]] Point<float> getPinPos (int index, bool isInput) const
        {
            for (auto* pin : pins)
                if (pin->pin.channelIndex == index && isInput == pin->isInput)
                    return getPosition().toFloat() + pin->getBounds().getCentre().toFloat();

            return {};
        }

        void update()
        {
            const AudioProcessorGraph::Node::Ptr f (graph.graph.getNodeForId (pluginID));
            jassert (f != nullptr);

            auto& processor = *f->getProcessor();

            numIns = processor.getTotalNumInputChannels();
            if (processor.acceptsMidi())
                ++numIns;

            numOuts = processor.getTotalNumOutputChannels();
            if (processor.producesMidi())
                ++numOuts;

            int w = 100;
            int h = 60;

            w = jmax (w, (jmax (numIns, numOuts) + 1) * 20);

            const int textWidth = font.getStringWidth (processor.getName());
            w = jmax (w, 16 + jmin (textWidth, 300));
            if (textWidth > 300)
                h = 100;

            setSize (w, h);
            setName (processor.getName());

            {
                auto p = graph.getNodePosition (pluginID);
                setCentreRelative ((float) p.x, (float) p.y);
            }

            if (numIns != numInputs || numOuts != numOutputs)
            {
                numInputs = numIns;
                numOutputs = numOuts;

                pins.clear();

                for (int i = 0; i < processor.getTotalNumInputChannels(); ++i)
                    addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, i }, true)));

                if (processor.acceptsMidi())
                    addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, AudioProcessorGraph::midiChannelIndex }, true)));

                for (int i = 0; i < processor.getTotalNumOutputChannels(); ++i)
                    addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, i }, false)));

                if (processor.producesMidi())
                    addAndMakeVisible (pins.add (new PinComponent (panel, { pluginID, AudioProcessorGraph::midiChannelIndex }, false)));

                resized();
            }
        }

        [[nodiscard]] AudioProcessor* getProcessor() const
        {
            if (auto node = graph.graph.getNodeForId (pluginID))
                return node->getProcessor();

            return {};
        }

        void showPopupMenu()
        {
            if(menu != nullptr)
                menu->dismissAllActiveMenus();
            menu = std::make_unique<PopupMenu> ();
            menu->addItem ("Delete this filter", [this] { graph.removeNode (pluginID); });
            menu->addItem ("Disconnect all pins", [this] { graph.disconnectNode(pluginID); });
            menu->addItem ("Toggle Bypass", [this]
                {
                    if (auto* node = graph.graph.getNodeForId (pluginID))
                        node->setBypassed (! node->isBypassed());

                    repaint();
                });

            menu->addSeparator();
            if (getProcessor()->hasEditor())
                menu->addItem ("Show GUI", [this] { showWindow (ModuleWindow::Type::normal); });

            menu->addItem ("Show all programs", [this] { showWindow (ModuleWindow::Type::programs); });
            menu->addItem ("Show all parameters", [this] { showWindow (ModuleWindow::Type::generic); });
            menu->addItem ("Show debug log", [this] { showWindow (ModuleWindow::Type::debug); });

            menu->addSeparator();
            menu->addItem ("Test state save/load", [this] { testStateSaveLoad(); });

            menu->addSeparator();
            menu->addItem ("Save plugin state", [this] { savePluginState(); });
            menu->addItem ("Load plugin state", [this] { loadPluginState(); });

            menu->showMenuAsync ({});
        }

        void testStateSaveLoad() const
        {
            if (auto* processor = getProcessor())
            {
                MemoryBlock state;
                processor->getStateInformation (state);
                processor->setStateInformation (state.getData(), (int) state.getSize());
            }
        }

        void showWindow (ModuleWindow::Type type)
        {
            if (auto node = graph.graph.getNodeForId (pluginID))
                if (auto* w = panel.getOrCreateWindowFor (node, type))
                    w->toFront (true);
        }

        void parameterValueChanged (int, float) override
        {
            // Parameter changes might come from the audio thread or elsewhere, but
            // we can only call repaint from the message thread.
            triggerAsyncUpdate();
        }

        void parameterGestureChanged (int, bool) override  {}

        void handleAsyncUpdate() override { repaint(); }

        void savePluginState()
        {
            fileChooser = std::make_unique<FileChooser> ("Save plugin state");

            const auto onChosen = [ref = SafePointer<PluginComponent> (this)] (const FileChooser& chooser)
            {
                if (ref == nullptr)
                    return;

                const auto result = chooser.getResult();

                if (result == File())
                    return;

                if (auto* node = ref->graph.graph.getNodeForId (ref->pluginID))
                {
                    MemoryBlock block;
                    node->getProcessor()->getStateInformation (block);
                    result.replaceWithData (block.getData(), block.getSize());
                }
            };

            fileChooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, onChosen);
        }

        void loadPluginState()
        {
            fileChooser = std::make_unique<FileChooser> ("Load plugin state");

            const auto onChosen = [ref = SafePointer<PluginComponent> (this)] (const FileChooser& chooser)
            {
                if (ref == nullptr)
                    return;

                const auto result = chooser.getResult();

                if (result == File())
                    return;

                if (auto* node = ref->graph.graph.getNodeForId (ref->pluginID))
                {
                    if (auto stream = result.createInputStream())
                    {
                        MemoryBlock block;
                        stream->readIntoMemoryBlock (block);
                        node->getProcessor()->setStateInformation (block.getData(), (int) block.getSize());
                    }
                }
            };

            fileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, onChosen);
        }

        GraphEditorPanel& panel;
        ProcessorGraph& graph;
        const AudioProcessorGraph::NodeID pluginID;
        OwnedArray<PinComponent> pins;
        int numInputs = 0, numOutputs = 0;
        int pinSize = 16;
        Point<int> originalPos;
        Font font { 13.0f, Font::bold };
        int numIns = 0, numOuts = 0;
        DropShadowEffect shadow;
        std::unique_ptr<PopupMenu> menu;
        std::unique_ptr<FileChooser> fileChooser;
    };


    //==============================================================================
    struct GraphEditorPanel::ConnectorComponent final : public Component,
                                                        public SettableTooltipClient
    {
        explicit ConnectorComponent (GraphEditorPanel& p)
            : panel (p), graph (p.graph)
        {
            setAlwaysOnTop (true);
        }

        void setInput (AudioProcessorGraph::NodeAndChannel newSource)
        {
            if (connection.source != newSource)
            {
                connection.source = newSource;
                update();
            }
        }

        void setOutput (AudioProcessorGraph::NodeAndChannel newDest)
        {
            if (connection.destination != newDest)
            {
                connection.destination = newDest;
                update();
            }
        }

        void dragStart (Point<float> pos)
        {
            lastInputPos = pos;
            resizeToFit();
        }

        void dragEnd (Point<float> pos)
        {
            lastOutputPos = pos;
            resizeToFit();
        }

        void update()
        {
            Point<float> p1, p2;
            getPoints (p1, p2);

            if (lastInputPos != p1 || lastOutputPos != p2)
                resizeToFit();
        }

        void resizeToFit()
        {
            Point<float> p1, p2;
            getPoints (p1, p2);

            auto newBounds = Rectangle<float> (p1, p2).expanded (4.0f).getSmallestIntegerContainer();

            if (newBounds != getBounds())
                setBounds (newBounds);
            else
                resized();

            repaint();
        }

        void getPoints (Point<float>& p1, Point<float>& p2) const
        {
            p1 = lastInputPos;
            p2 = lastOutputPos;

            if (auto* src = panel.getComponentForPlugin (connection.source.nodeID))
                p1 = src->getPinPos (connection.source.channelIndex, false);

            if (auto* dest = panel.getComponentForPlugin (connection.destination.nodeID))
                p2 = dest->getPinPos (connection.destination.channelIndex, true);
        }

        void paint (Graphics& g) override
        {
            if (connection.source.isMIDI() || connection.destination.isMIDI())
                g.setColour (Colours::red);
            else
                g.setColour (Colours::green);

            g.fillPath (linePath);
        }

        bool hitTest (int x, int y) override
        {
            auto pos = Point<int> (x, y).toFloat();

            if (hitPath.contains (pos))
            {
                double distanceFromStart, distanceFromEnd;
                getDistancesFromEnds (pos, distanceFromStart, distanceFromEnd);

                // avoid clicking the connector when over a pin
                return distanceFromStart > 7.0 && distanceFromEnd > 7.0;
            }

            return false;
        }

        void mouseDown (const MouseEvent&) override
        {
            dragging = false;
        }

        void mouseDrag (const MouseEvent& e) override
        {
            if (dragging)
            {
                panel.dragConnector (e);
            }
            else if (e.mouseWasDraggedSinceMouseDown())
            {
                dragging = true;

                graph.removeConnection (connection);

                double distanceFromStart, distanceFromEnd;
                getDistancesFromEnds (getPosition().toFloat() + e.position, distanceFromStart, distanceFromEnd);
                const bool isNearerSource = (distanceFromStart < distanceFromEnd);

                AudioProcessorGraph::NodeAndChannel dummy { {}, 0 };

                panel.beginConnectorDrag (isNearerSource ? dummy : connection.source,
                    isNearerSource ? connection.destination : dummy,
                    e);
            }
        }

        void mouseUp (const MouseEvent& e) override
        {
            if (dragging)
                panel.endDraggingConnector (e);
        }

        void resized() override
        {
            Point<float> p1, p2;
            getPoints (p1, p2);

            lastInputPos = p1;
            lastOutputPos = p2;

            p1 -= getPosition().toFloat();
            p2 -= getPosition().toFloat();

            linePath.clear();
            linePath.startNewSubPath (p1);
            linePath.cubicTo (p1.x, p1.y + (p2.y - p1.y) * 0.33f,
                p2.x, p1.y + (p2.y - p1.y) * 0.66f,
                p2.x, p2.y);

            PathStrokeType wideStroke (8.0f);
            wideStroke.createStrokedPath (hitPath, linePath);

            PathStrokeType stroke (2.5f);
            stroke.createStrokedPath (linePath, linePath);

            auto arrowW = 5.0f;
            auto arrowL = 4.0f;

            Path arrow;
            arrow.addTriangle (-arrowL, arrowW,
                -arrowL, -arrowW,
                arrowL, 0.0f);

            arrow.applyTransform (AffineTransform()
                                      .rotated (MathConstants<float>::halfPi - (float) atan2 (p2.x - p1.x, p2.y - p1.y))
                                      .translated ((p1 + p2) * 0.5f));

            linePath.addPath (arrow);
            linePath.setUsingNonZeroWinding (true);
        }

        void getDistancesFromEnds (Point<float> p, double& distanceFromStart, double& distanceFromEnd) const
        {
            Point<float> p1, p2;
            getPoints (p1, p2);

            distanceFromStart = p1.getDistanceFrom (p);
            distanceFromEnd   = p2.getDistanceFrom (p);
        }

        GraphEditorPanel& panel;
        ProcessorGraph& graph;
        AudioProcessorGraph::Connection connection { { {}, 0 }, { {}, 0 } };
        Point<float> lastInputPos, lastOutputPos;
        Path linePath, hitPath;
        bool dragging = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectorComponent)
    };


    //==============================================================================
    GraphEditorPanel::GraphEditorPanel (ProcessorGraph& g)  : graph (g)
    {
        graph.addListener(this);
        graph.graph.addChangeListener (this);
        setOpaque (true);
        graph.onProcessorWindowRequested = [this] (AudioProcessorGraph::Node::Ptr node, ModuleWindow::Type type) -> ModuleWindow*
        {
            if(graph.guiConfig.enableProcessorEditorCreation)
                return getOrCreateWindowFor (node, type);

            return nullptr;
        };
    }

    GraphEditorPanel::~GraphEditorPanel()
    {
        graph.onProcessorWindowRequested = nullptr;
        graph.removeListener(this);
        graph.graph.removeChangeListener(this);
        draggingConnector = nullptr;
        nodes.clear();
        connectors.clear();
    }

    void GraphEditorPanel::paint (Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void GraphEditorPanel::mouseDown (const MouseEvent& e)
    {
        if (e.mods.isPopupMenu() && graph.guiConfig.enableProcessorCreationMenu)
            showPopupMenu (e.position.toInt());
    }

    GraphEditorPanel::PluginComponent* GraphEditorPanel::getComponentForPlugin (AudioProcessorGraph::NodeID nodeID) const
    {
        for (auto* fc : nodes)
            if (fc->pluginID == nodeID)
                return fc;

        return nullptr;
    }

    GraphEditorPanel::ConnectorComponent* GraphEditorPanel::getComponentForConnection (const AudioProcessorGraph::Connection& conn) const
    {
        for (auto* cc : connectors)
            if (cc->connection == conn)
                return cc;

        return nullptr;
    }

    GraphEditorPanel::PinComponent* GraphEditorPanel::findPinAt (Point<float> pos) const
    {
        for (auto* fc : nodes)
        {
            // NB: A Visual Studio optimiser error means we have to put this Component* in a local
            // variable before trying to cast it, or it gets mysteriously optimised away..
            auto* comp = fc->getComponentAt (pos.toInt() - fc->getPosition());

            if (auto* pin = dynamic_cast<PinComponent*> (comp))
                return pin;
        }

        return nullptr;
    }

    void GraphEditorPanel::resized()
    {
        updateComponents();
    }

    void GraphEditorPanel::changeListenerCallback (ChangeBroadcaster*)
    {
        updateComponents();

        for (int i = activeModuleWindows.size(); --i >= 0;)
            if (! graph.graph.getNodes().contains (activeModuleWindows.getUnchecked (i)->node))
                activeModuleWindows.remove (i);
    }

    bool GraphEditorPanel::closeAnyOpenModuleWindows()
    {
        bool wasEmpty = activeModuleWindows.isEmpty();
        activeModuleWindows.clear();
        return ! wasEmpty;
    }

    void GraphEditorPanel::graphIsAboutToBeCleared()
    {
        closeAnyOpenModuleWindows();
    }

    ModuleWindow* GraphEditorPanel::getOrCreateWindowFor (AudioProcessorGraph::Node* node, ModuleWindow::Type type)
    {
        if(node == nullptr)
            return nullptr;

        for (auto* w : activeModuleWindows)
            if (w->node.get() == node && w->type == type)
                return w;

        if (auto* processor = node->getProcessor())
        {
            if (! processor->hasEditor())
            {
                return nullptr;
            }
            return activeModuleWindows.add (new ModuleWindow (node, type, activeModuleWindows));
        }

        return nullptr;
    }

    void GraphEditorPanel::updateComponents()
    {
        for (int i = nodes.size(); --i >= 0;)
            if (graph.graph.getNodeForId (nodes.getUnchecked (i)->pluginID) == nullptr)
                nodes.remove (i);

        for (int i = connectors.size(); --i >= 0;)
            if (! graph.graph.isConnected (connectors.getUnchecked (i)->connection))
                connectors.remove (i);

        for (auto* fc : nodes)
            fc->update();

        for (auto* cc : connectors)
            cc->update();

        for (auto* f : graph.graph.getNodes())
        {
            if (getComponentForPlugin (f->nodeID) == nullptr)
            {
                auto* comp = nodes.add (new PluginComponent (*this, f->nodeID));
                addAndMakeVisible (comp);
                comp->update();
            }
        }

        for (auto& c : graph.graph.getConnections())
        {
            if (getComponentForConnection (c) == nullptr)
            {
                auto* comp = connectors.add (new ConnectorComponent (*this));
                addAndMakeVisible (comp);

                comp->setInput (c.source);
                comp->setOutput (c.destination);
            }
        }
    }

    void GraphEditorPanel::showPopupMenu (Point<int> mousePos)
    {
        if (menu != nullptr)
            menu->dismissAllActiveMenus();
        menu = std::make_unique<PopupMenu> ();

        if (findParentComponentOfClass<GraphEditor>())
        {
            addPluginsToMenu (*menu);

            menu->showMenuAsync ({},
                ModalCallbackFunction::create ([this, mousePos] (int r)
                    {
                        if (findParentComponentOfClass<GraphEditor>() && r > 0)
                        {
                            const auto point = mousePos.toDouble() / Point<double> ((double) getWidth(), (double) getHeight());
                            graph.createModule(r - 1, point.getX(), point.getY());
                        }
                    }));
        }
    }

    void GraphEditorPanel::addPluginsToMenu (PopupMenu& m) const
    {
        const auto names = graph.factory.getNames();
        auto menuID = 1;
        for (auto& name : names)
        {
            m.addItem (menuID, name, true, false);
            menuID++;
        }
    }

    void GraphEditorPanel::beginConnectorDrag (AudioProcessorGraph::NodeAndChannel source,
        AudioProcessorGraph::NodeAndChannel dest,
        const MouseEvent& e)
    {
        auto* c = dynamic_cast<ConnectorComponent*> (e.originalComponent);
        connectors.removeObject (c, false);
        draggingConnector.reset (c);

        if (draggingConnector == nullptr)
            draggingConnector = std::make_unique<ConnectorComponent> (*this);

        draggingConnector->setInput (source);
        draggingConnector->setOutput (dest);

        addAndMakeVisible (draggingConnector.get());
        draggingConnector->toFront (false);

        dragConnector (e);
    }

    void GraphEditorPanel::dragConnector (const MouseEvent& e)
    {
        auto e2 = e.getEventRelativeTo (this);

        if (draggingConnector != nullptr)
        {
            draggingConnector->setTooltip ({});

            auto pos = e2.position;

            if (auto* pin = findPinAt (pos))
            {
                auto connection = draggingConnector->connection;

                if (connection.source.nodeID == AudioProcessorGraph::NodeID() && ! pin->isInput)
                {
                    connection.source = pin->pin;
                }
                else if (connection.destination.nodeID == AudioProcessorGraph::NodeID() && pin->isInput)
                {
                    connection.destination = pin->pin;
                }

                if (graph.graph.canConnect (connection))
                {
                    pos = (pin->getParentComponent()->getPosition() + pin->getBounds().getCentre()).toFloat();
                    draggingConnector->setTooltip (pin->getTooltip());
                }
            }

            if (draggingConnector->connection.source.nodeID == AudioProcessorGraph::NodeID())
                draggingConnector->dragStart (pos);
            else
                draggingConnector->dragEnd (pos);
        }
    }

    void GraphEditorPanel::endDraggingConnector (const MouseEvent& e)
    {
        if (draggingConnector == nullptr)
            return;

        draggingConnector->setTooltip ({});

        auto e2 = e.getEventRelativeTo (this);
        auto connection = draggingConnector->connection;

        draggingConnector = nullptr;

        if (auto* pin = findPinAt (e2.position))
        {
            if (connection.source.nodeID == AudioProcessorGraph::NodeID())
            {
                if (pin->isInput)
                    return;

                connection.source = pin->pin;
            }
            else
            {
                if (! pin->isInput)
                    return;

                connection.destination = pin->pin;
            }

            graph.addConnection (connection);
        }
    }

    //==============================================================================
    struct GraphDocumentComponent::TooltipBar final : public Component,
                                                      private Timer
    {
        TooltipBar()
        {
            startTimer (100);
        }

        void paint (Graphics& g) override
        {
            g.setFont (Font ((float) getHeight() * 0.7f, Font::bold));
            g.setColour (Colours::black);
            g.drawFittedText (tip, 10, 0, getWidth() - 12, getHeight(), Justification::centredLeft, 1);
        }

        void timerCallback() override
        {
            String newTip;

            if (auto* underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
                if (auto* ttc = dynamic_cast<TooltipClient*> (underMouse))
                    if (! (underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
                        newTip = ttc->getTooltip();

            if (newTip != tip)
            {
                tip = newTip;
                repaint();
            }
        }

        String tip;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipBar)
    };

    //==============================================================================
    GraphDocumentComponent::GraphDocumentComponent (ProcessorGraph& g)
        : graph (g)
    {
        init();
    }

    void GraphDocumentComponent::init()
    {
        graphPanel = std::make_unique<GraphEditorPanel> (graph);
        addAndMakeVisible (graphPanel.get());

        statusBar = std::make_unique<TooltipBar> ();
        addAndMakeVisible (statusBar.get());

        graphPanel->updateComponents();
    }

    GraphDocumentComponent::~GraphDocumentComponent()
    = default;

    void GraphDocumentComponent::resized()
    {
        auto r = [this]
        {
            auto bounds = getLocalBounds();

            if (auto* display = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
                return display->safeAreaInsets.subtractedFrom (bounds);

            return bounds;
        }();

        const int statusHeight = 20;

        statusBar->setBounds (r.removeFromBottom (statusHeight));
        graphPanel->setBounds (r);
    }

    GraphEditor::GraphEditor (juce::AudioProcessor& p, ProcessorGraph& g)
        : AudioProcessorEditor (&p),
          audioProcessor (p),
          graphDocumentComponent(g)
    {
        addAndMakeVisible(graphDocumentComponent);
        setSize (600, 400);
    }

    GraphEditor::~GraphEditor()= default;

    void GraphEditor::resized()
    {
        auto b = getLocalBounds();
        graphDocumentComponent.setBounds (b);
    }
} // namespace PlayfulTones
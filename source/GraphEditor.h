//
// Created by Bence Kovács on 04/01/2024.
//

#pragma once
using namespace juce;
namespace PlayfulTones {
    //==============================================================================
    /**
        A panel that displays and edits a ProcessorGraph.
    */
    class GraphEditorPanel final : public Component,
                                   public ChangeListener,
                                   public Button::Listener,
                                   private ProcessorGraph::Listener
    {
    public:
        //==============================================================================
        explicit GraphEditorPanel (ProcessorGraph& graph);
        void buttonClicked(Button* button) override;
        ~GraphEditorPanel() override;

        void paint (Graphics&) override;
        void resized() override;

        void mouseDown (const MouseEvent&) override;

        void changeListenerCallback (ChangeBroadcaster*) override;

        //==============================================================================
        void updateComponents();
        ModuleWindow* getOrCreateWindowFor (const AudioProcessorGraph::Node::Ptr&, ModuleWindow::Type);
        bool closeAnyOpenModuleWindows();

        void graphIsAboutToBeCleared () override;

        //==============================================================================
        void showPopupMenu (Point<int> position);

        //==============================================================================
        void beginConnectorDrag (AudioProcessorGraph::NodeAndChannel source,
            AudioProcessorGraph::NodeAndChannel dest,
            const MouseEvent&);
        void dragConnector (const MouseEvent&);
        void endDraggingConnector (const MouseEvent&);

        //==============================================================================
        ProcessorGraph& graph;

    private:
        struct PluginComponent;
        struct ConnectorComponent;
        struct PinComponent;

        OwnedArray<PluginComponent> nodes;
        OwnedArray<ConnectorComponent> connectors;
        std::unique_ptr<ConnectorComponent> draggingConnector;
        std::unique_ptr<PopupMenu> menu;
        OwnedArray<ModuleWindow> activeModuleWindows;
        
        // Embedded editor components
        std::unique_ptr<TextButton> backButton;
        std::unique_ptr<Label> processorNameLabel;
        std::unique_ptr<juce::AudioProcessorEditor> currentEditor;
        AudioProcessorGraph::Node::Ptr currentNode;
        static inline const juce::String embeddedEditorNodeId = "embeddedEditorNodeId";

        [[nodiscard]] PluginComponent* getComponentForPlugin (AudioProcessorGraph::NodeID) const;
        [[nodiscard]] ConnectorComponent* getComponentForConnection (const AudioProcessorGraph::Connection&) const;
        [[nodiscard]] PinComponent* findPinAt (Point<float>) const;

        void addPluginsToMenu (PopupMenu& m) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
    };


    //==============================================================================
    class GraphDocumentComponent final : public Component
    {
    public:
        explicit GraphDocumentComponent (ProcessorGraph& g);

        ~GraphDocumentComponent() override;

        //==============================================================================
        void resized() override;

        //==============================================================================
        std::unique_ptr<GraphEditorPanel> graphPanel;
        ProcessorGraph& graph;

    private:
        struct TooltipBar;
        std::unique_ptr<TooltipBar> statusBar;

        //==============================================================================

        void init();

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphDocumentComponent)
    };

    class GraphEditor : public juce::AudioProcessorEditor
    {
    public:
        GraphEditor (juce::AudioProcessor& p, ProcessorGraph& g);
        ~GraphEditor() override;

    private:
        GraphDocumentComponent graphDocumentComponent;

        void resized() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphEditor)
    };

    class GraphWindow final : public DocumentWindow
    {
    public:
        GraphWindow (ProcessorGraph& graph);
        ~GraphWindow() override;

        void closeButtonPressed() override;
    private:
        GraphDocumentComponent graphDocumentComponent;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphWindow)
    };
} // namespace PlayfulTones

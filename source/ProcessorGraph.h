//
// Created by Bence Kovács on 04/01/2024.
//

#pragma once
using namespace juce;
namespace PlayfulTones {
    class ProcessorGraph
    {
    public:
        //==============================================================================

        /**
            Configuration options for the GUI.
        */
        struct GuiConfig
        {
            GuiConfig()
            : enableProcessorCreationMenu(true)
            , enableProcessorContextMenu(true)
            , enableProcessorEditorCreation(true)
            {}

            /*
             * Allow the creation of new processors from the context menu (by right-clicking on the background).
             */
            bool enableProcessorCreationMenu;
            /*
             * Allow the context menu to be opened for processors (by right-clicking on the node).
             */
            bool enableProcessorContextMenu;
            /*
             * Allow the creation of new editor windows for processors (by double-clicking on the node).
             */
            bool enableProcessorEditorCreation;
        };


        //==============================================================================
        ProcessorGraph (ModuleFactory& factory, GuiConfig guiConfig = GuiConfig());
        ~ProcessorGraph();

        //==============================================================================
        using NodeID = AudioProcessorGraph::NodeID;

        void setNodePosition (NodeID, Point<double>) const;
        Point<double> getNodePosition (NodeID) const;

        //==============================================================================
        void clear();

        //==============================================================================
        std::unique_ptr<XmlElement> createXml() const;
        void restoreFromXml (const XmlElement&);

        juce::AudioProcessorGraph::Node::Ptr createModule (int factoryId, double x = .5, double y = .5);
        void addConnection(const AudioProcessorGraph::Connection&);
        void removeConnection(const AudioProcessorGraph::Connection&);
        void removeNode(NodeID);
        void removeNode(const AudioProcessorGraph::Node::Ptr&);

        void disconnectNode(NodeID);
        void disconnectNode(const AudioProcessorGraph::Node::Ptr&);

        //==============================================================================

        /**
            Used to receive callbacks when the graph's state changes.

        @see ProcessorGraph::addListener, ProcessorGraph::removeListener
        */
        class Listener
        {
        public:
            virtual ~Listener() = default;
            virtual void nodeAdded (NodeID) {}
            virtual void nodeRemoved (NodeID) {}
            virtual void connectionAdded (const AudioProcessorGraph::Connection&) {}
            virtual void connectionRemoved (const AudioProcessorGraph::Connection&) {}
            virtual void graphIsAboutToBeCleared() {}
        };

        /** Registers a listener to receive events when this graph's state changes.
            If the listener is already registered, this will not register it again.
            @see removeListener
        */
        void addListener (Listener* newListener);

        /** Removes a previously-registered graph listener
            @see addListener
        */
        void removeListener (Listener* listener);

        /** Called when a new editor window is requested for a processor.
        */
        std::function<ModuleWindow* (AudioProcessorGraph::Node::Ptr, ModuleWindow::Type)> onProcessorWindowRequested;

        //==============================================================================
        AudioProcessorGraph graph;
        ModuleFactory& factory;
        const GuiConfig guiConfig;

        //==============================================================================
        static inline const juce::String xPosId = "x";
        static inline const juce::String yPosId = "y";
        static inline const juce::String factoryId = "factoryId";
        static inline const juce::String nodeId = "uid";

        static inline const juce::String valueTag = "value";
        static inline const juce::String nameTag = "name";
        static inline const juce::String typeTag = "type";

        static inline const juce::String boolValue = "bool";
        static inline const juce::String intValue = "int";
        static inline const juce::String floatValue = "float";
        static inline const juce::String stringValue = "string";

        static inline const juce::String stateAttrName = "STATE";
        static inline const juce::String propertyAttrName = "PROPERTY";
        static inline const juce::String graphAttrName = "FILTERGRAPH";
        static inline const juce::String connectionAttrName = "CONNECTION";
        static inline const juce::String srcFilterAttrName = "srcFilter";
        static inline const juce::String srcChannelAttrName = "srcChannel";
        static inline const juce::String dstFilterAttrName = "dstFilter";
        static inline const juce::String dstChannelAttrName = "dstChannel";
        static inline const juce::String layoutAttrName = "LAYOUT";
        static inline const juce::String filterAttrName = "FILTER";
        static inline const juce::String inputsAttrName = "INPUTS";
        static inline const juce::String outputsAttrName = "OUTPUTS";
        static inline const juce::String busAttrName = "BUS";
        static inline const juce::String indexAttrName = "index";
        static inline const juce::String disabledAttrValue = "disabled";

    private:
        //==============================================================================

        AudioProcessorGraph::Node::Ptr createNodeFromXml (const XmlElement&);

        XmlElement restoredState { "RestoredState" };

        ListenerList<Listener> graphListeners;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)
    };
} // namespace PlayfulTones
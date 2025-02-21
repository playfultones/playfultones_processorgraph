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
            GuiConfig() {}

            [[nodiscard]] GuiConfig withProcessorCreationMenu(bool enabled) const
            {
                auto copy = *this;
                copy.enableProcessorCreationMenu = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withProcessorContextMenu(bool enabled) const
            {
                auto copy = *this;
                copy.enableProcessorContextMenu = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withProcessorEditorCreation(bool enabled) const
            {
                auto copy = *this;
                copy.enableProcessorEditorCreation = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withEditorInSameWindow(bool enabled) const
            {
                auto copy = *this;
                copy.editorOpensInSameWindow = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withNodeConnectionModification(bool enabled) const
            {
                auto copy = *this;
                copy.nodeConnectionsCanBeModified = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withNodePositionModification(bool enabled) const
            {
                auto copy = *this;
                copy.nodePositionsCanBeModified = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withNodeDeletion(bool enabled) const
            {
                auto copy = *this;
                copy.enableNodeDeletion = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withNodeDisconnection(bool enabled) const
            {
                auto copy = *this;
                copy.enableNodeDisconnection = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withNodeBypass(bool enabled) const
            {
                auto copy = *this;
                copy.enableNodeBypass = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withShowGUI(bool enabled) const
            {
                auto copy = *this;
                copy.enableShowGUI = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withShowPrograms(bool enabled) const
            {
                auto copy = *this;
                copy.enableShowPrograms = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withShowParameters(bool enabled) const
            {
                auto copy = *this;
                copy.enableShowParameters = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withShowDebugLog(bool enabled) const
            {
                auto copy = *this;
                copy.enableShowDebugLog = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withTestStateSaveLoad(bool enabled) const
            {
                auto copy = *this;
                copy.enableTestStateSaveLoad = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withPluginStateSave(bool enabled) const
            {
                auto copy = *this;
                copy.enablePluginStateSave = enabled;
                return copy;
            }

            [[nodiscard]] GuiConfig withPluginStateLoad(bool enabled) const
            {
                auto copy = *this;
                copy.enablePluginStateLoad = enabled;
                return copy;
            }

            /*
             * Allow the creation of new processors from the context menu (by right-clicking on the background).
             */
            bool enableProcessorCreationMenu = true;
            /*
             * Allow the context menu to be opened for processors (by right-clicking on the node).
             */
            bool enableProcessorContextMenu = true;
            /*
             * Allow the creation of new editor windows for processors (by double-clicking on the node).
             */
            bool enableProcessorEditorCreation = true;

            /*
             * Open the editor in the same window that hosts the graph view.
             */
            bool editorOpensInSameWindow = false;

            /*
             * Nodes in the graph can be manually connected/disconnected in the graph view.
             */
            bool nodeConnectionsCanBeModified = true;

            /*
             * The position of nodes can be manually modified in the graph view.
             */
            bool nodePositionsCanBeModified = true;

            /*
             * Allow deleting nodes from the context menu
             */
            bool enableNodeDeletion = true;

            /*
             * Allow disconnecting all pins from the context menu
             */
            bool enableNodeDisconnection = true;

            /*
             * Allow bypassing nodes from the context menu
             */
            bool enableNodeBypass = true;

            /*
             * Allow showing the GUI editor from the context menu
             */
            bool enableShowGUI = true;

            /*
             * Allow showing all programs from the context menu
             */
            bool enableShowPrograms = true;

            /*
             * Allow showing all parameters from the context menu
             */
            bool enableShowParameters = true;

            /*
             * Allow showing debug log from the context menu
             */
            bool enableShowDebugLog = true;

            /*
             * Allow testing state save/load from the context menu
             */
            bool enableTestStateSaveLoad = true;

            /*
             * Allow saving plugin state from the context menu
             */
            bool enablePluginStateSave = true;

            /*
             * Allow loading plugin state from the context menu
             */
            bool enablePluginStateLoad = true;
        };


        //==============================================================================
        explicit ProcessorGraph (ModuleFactory& factory, GuiConfig guiConfig = GuiConfig());
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
        static inline const juce::String instanceId = "nodeInstanceId";
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

        std::map<int, int> factoryIdToNextInstanceIdMap;
        int getNextInstanceId(int factoryId);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorGraph)
    };
} // namespace PlayfulTones

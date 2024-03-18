//
// Created by Bence Kovács on 04/01/2024.
//

using namespace juce;
namespace PlayfulTones {
    class ProcessorGraph;

    /**
        A window that shows a log of parameter change messages sent by the plugin.
    */
    class ModuleDebugWindow final : public AudioProcessorEditor,
                                    public AudioProcessorParameter::Listener,
                                    public ListBoxModel,
                                    public AsyncUpdater
    {
    public:
        explicit ModuleDebugWindow (AudioProcessor& proc)
            : AudioProcessorEditor (proc), audioProc (proc)
        {
            setSize (500, 200);
            addAndMakeVisible (list);

            for (auto* p : audioProc.getParameters())
                p->addListener (this);

            log.add ("Parameter debug log started");
        }

        ~ModuleDebugWindow() override
        {
            for (auto* p : audioProc.getParameters())
                p->removeListener (this);
        }

        void parameterValueChanged (int parameterIndex, float newValue) override
        {
            auto* param = audioProc.getParameters()[parameterIndex];
            auto value = param->getCurrentValueAsText().quoted() + " (" + String (newValue, 4) + ")";

            appendToLog ("parameter change", *param, value);
        }

        void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override
        {
            auto* param = audioProc.getParameters()[parameterIndex];
            appendToLog ("gesture", *param, gestureIsStarting ? "start" : "end");
        }

    private:
        void appendToLog (StringRef action, AudioProcessorParameter& param, StringRef value)
        {
            String entry (action + " " + param.getName (30).quoted() + " [" + String (param.getParameterIndex()) + "]: " + value);

            {
                ScopedLock lock (pendingLogLock);
                pendingLogEntries.add (entry);
            }

            triggerAsyncUpdate();
        }

        void resized() override
        {
            list.setBounds (getLocalBounds());
        }

        int getNumRows() override
        {
            return log.size();
        }

        void paintListBoxItem (int rowNumber, Graphics& g, int width, int height, bool) override
        {
            g.setColour (getLookAndFeel().findColour (TextEditor::textColourId));

            if (isPositiveAndBelow (rowNumber, log.size()))
                g.drawText (log[rowNumber], Rectangle<int> { 0, 0, width, height }, Justification::left, true);
        }

        void handleAsyncUpdate() override
        {
            if (log.size() > logSizeTrimThreshold)
                log.removeRange (0, log.size() - maxLogSize);

            {
                ScopedLock lock (pendingLogLock);
                log.addArray (pendingLogEntries);
                pendingLogEntries.clear();
            }

            list.updateContent();
            list.scrollToEnsureRowIsOnscreen (log.size() - 1);
        }

        constexpr static const int maxLogSize = 300;
        constexpr static const int logSizeTrimThreshold = 400;

        ListBox list { "Log", this };

        StringArray log;
        StringArray pendingLogEntries;
        CriticalSection pendingLogLock;

        AudioProcessor& audioProc;
    };

    //==============================================================================
    /**
        A desktop window containing a plugin's GUI.
    */
    class ModuleWindow final : public DocumentWindow
    {
    public:
        enum class Type
        {
            normal = 0,
            generic,
            programs,
            debug,
            first = normal,
            last = debug,
        };

        ModuleWindow (AudioProcessorGraph::Node* n, Type t, OwnedArray<ModuleWindow>& windowList)
            : DocumentWindow (n->getProcessor()->getName(),
                LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                DocumentWindow::minimiseButton | DocumentWindow::closeButton),
              activeWindowList (windowList),
              node (n), type (t)
        {
            setSize (400, 300);

            if (auto* ui = createProcessorEditor (*node->getProcessor(), type))
            {
                setContentOwned (ui, true);
                setResizable (ui->isResizable(), false);
            }

            setConstrainer (&constrainer);

            setTopLeftPosition (node->properties.getWithDefault (getLastXProp (type), Random::getSystemRandom().nextInt (500)),
                node->properties.getWithDefault (getLastYProp (type), Random::getSystemRandom().nextInt (500)));

            node->properties.set (getOpenProp (type), true);

            DocumentWindow::setVisible (true);
        }

        ~ModuleWindow() override
        {
            node->getProcessor()->editorBeingDeleted (dynamic_cast<AudioProcessorEditor*> (getContentComponent()));
            clearContentComponent();
        }

        void moved() override
        {
            node->properties.set (getLastXProp (type), getX());
            node->properties.set (getLastYProp (type), getY());
        }

        void closeButtonPressed() override
        {
            node->properties.set (getOpenProp (type), false);
            activeWindowList.removeObject (this);
        }

        static String getLastXProp (Type type)    { return "uiLastX_" + getTypeName (type); }
        static String getLastYProp (Type type)    { return "uiLastY_" + getTypeName (type); }
        static String getOpenProp  (Type type)    { return "uiopen_"  + getTypeName (type); }

        OwnedArray<ModuleWindow>& activeWindowList;
        const AudioProcessorGraph::Node::Ptr node;
        const Type type;

    private:
        class DecoratorConstrainer final : public BorderedComponentBoundsConstrainer
        {
        public:
            explicit DecoratorConstrainer (DocumentWindow& windowIn)
                : window (windowIn) {}

            [[nodiscard]] ComponentBoundsConstrainer* getWrappedConstrainer() const override
            {
                auto* editor = dynamic_cast<AudioProcessorEditor*> (window.getContentComponent());
                return editor != nullptr ? editor->getConstrainer() : nullptr;
            }

            [[nodiscard]] BorderSize<int> getAdditionalBorder() const override
            {
                const auto nativeFrame = [&]() -> BorderSize<int>
                {
                    if (auto* peer = window.getPeer())
                        if (const auto frameSize = peer->getFrameSizeIfPresent())
                            return *frameSize;

                    return {};
                }();

                return nativeFrame.addedTo (window.getContentComponentBorder());
            }

        private:
            DocumentWindow& window;
        };

        DecoratorConstrainer constrainer { *this };

        [[nodiscard]] float getDesktopScaleFactor() const override     { return 1.0f; }

        static AudioProcessorEditor* createProcessorEditor (AudioProcessor& processor,
            ModuleWindow::Type type)
        {
            if (type == ModuleWindow::Type::normal)
            {
                if (processor.hasEditor())
                    if (auto* ui = processor.createEditorIfNeeded())
                        return ui;

                type = ModuleWindow::Type::generic;
            }

            if (type == ModuleWindow::Type::generic)
            {
                auto* result = new GenericAudioProcessorEditor (processor);
                result->setResizeLimits (200, 300, 1'000, 10'000);
                return result;
            }

            if (type == ModuleWindow::Type::programs)
                return new ProgramAudioProcessorEditor (processor);

            if (type == ModuleWindow::Type::debug)
                return new ModuleDebugWindow (processor);

            jassertfalse;
            return {};
        }

        static String getTypeName (Type type)
        {
            switch (type)
            {
                case Type::normal:     return "Normal";
                case Type::generic:    return "Generic";
                case Type::programs:   return "Programs";
                case Type::debug:      return "Debug";
                default:               return {};
            }
        }

        //==============================================================================
        struct ProgramAudioProcessorEditor final : public AudioProcessorEditor
        {
            explicit ProgramAudioProcessorEditor (AudioProcessor& p)
                : AudioProcessorEditor (p)
            {
                setOpaque (true);

                addAndMakeVisible (listBox);
                listBox.updateContent();

                const auto rowHeight = listBox.getRowHeight();

                setSize (400, jlimit (rowHeight, 400, p.getNumPrograms() * rowHeight));
            }

            void paint (Graphics& g) override
            {
                g.fillAll (Colours::grey);
            }

            void resized() override
            {
                listBox.setBounds (getLocalBounds());
            }

        private:
            class Model : public ListBoxModel
            {
            public:
                Model (Component& o, AudioProcessor& p)
                    : owner (o), proc (p) {}

                int getNumRows() override
                {
                    return proc.getNumPrograms();
                }

                void paintListBoxItem (int rowNumber,
                    Graphics& g,
                    int width,
                    int height,
                    bool rowIsSelected) override
                {
                    const auto textColour = owner.findColour (ListBox::textColourId);

                    if (rowIsSelected)
                    {
                        const auto defaultColour = owner.findColour (ListBox::backgroundColourId);
                        const auto c = defaultColour.interpolatedWith (textColour, 0.5f);

                        g.fillAll (c);
                    }

                    g.setColour (textColour);
                    g.drawText (proc.getProgramName (rowNumber),
                        Rectangle<int> { width, height }.reduced (2),
                        Justification::left,
                        true);
                }

                void selectedRowsChanged (int row) override
                {
                    if (0 <= row)
                        proc.setCurrentProgram (row);
                }

            private:
                Component& owner;
                AudioProcessor& proc;
            };

            Model model { *this, *getAudioProcessor() };
            ListBox listBox { "Programs", &model };

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleWindow)
    };
} // namespace PlayfulTones